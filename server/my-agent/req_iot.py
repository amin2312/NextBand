import base64
import json
import logging
import time
from datetime import datetime
from aiodocker.containers import DockerContainer
from langcodes import Language
import asyncio
import paramiko

import schemas

import aiodocker
from aiodocker.exceptions import DockerError
from pathlib import Path
from conf import Conf, IsLocalhost
import os
import uuid
import httpx

from utils import agent_print, ReplacePlaceholder

from req_base import HTTPError, CrosHandler, ParseHandler


class User:
    def __init__(self, uid: str, mac: str):
        self.uid = uid
        self.mac = mac
        self.wants = ""
        self.access_token = str(uuid.uuid4())


Users: dict[str, User] = {}
Workflows: dict[str, DockerContainer] = {}


async def exec_shell(CMD: list) -> tuple[str, str]:
    if IsLocalhost:
        ssh = paramiko.SSHClient()
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh.connect(hostname=os.getenv('VPS1_HOST') or '', port=12369, username="root", password=os.getenv('VPS1_PASS') or '')
        _, stdout, stderr = ssh.exec_command(' '.join(CMD))
        stdout_text = stdout.read().decode('utf-8', errors='replace').strip()
        stderr_text = stderr.read().decode('utf-8', errors='replace').strip()
        exit_code = stdout.channel.recv_exit_status()
        ssh.close()
    else:
        proc = await asyncio.create_subprocess_exec(*CMD, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        stdout, stderr = await proc.communicate()
        stdout_text = stdout.decode('utf-8', errors='replace').strip()
        stderr_text = stderr.decode('utf-8', errors='replace').strip()
        exit_code = proc.returncode or 0
    if exit_code != 0:
        raise Exception(stderr_text or stdout_text or f'exited code: {exit_code}')
    return stdout_text, stderr_text


def build_script(PRE_STMT: str, SYSTEM_PROMPT: str, USER_PROMPT: str,  PROVIDER: str, MODEL: str, AT_FILES: str):
    return f"""
SYSTEM_PROMPT=$(cat <<'EOF_kmemNCN6kXCCF47ZFEfkRnz6'
{SYSTEM_PROMPT}
EOF_kmemNCN6kXCCF47ZFEfkRnz6
)
USER_PROMPT=$(cat <<'EOF_kmemNCN6kXCCF47ZFEfkRnz6'
{USER_PROMPT}
EOF_kmemNCN6kXCCF47ZFEfkRnz6
)
PROVIDER=$(cat <<'EOF_kmemNCN6kXCCF47ZFEfkRnz6'
{PROVIDER}
EOF_kmemNCN6kXCCF47ZFEfkRnz6
)
MODEL=$(cat <<'EOF_kmemNCN6kXCCF47ZFEfkRnz6'
{MODEL}
EOF_kmemNCN6kXCCF47ZFEfkRnz6
)

{PRE_STMT}
pi -p "$USER_PROMPT" --provider "$PROVIDER" --model "$MODEL" --append-system-prompt "$SYSTEM_PROMPT" --mode json --approve --no-session {AT_FILES}
"""


def build_docker_config(uid: int, image: str, timeout: str, script: str, env: list, isAutoRemove: bool, binds: list) -> dict:
    config = {
        "User": str(uid),
        "Image": image,
        "Cmd": ["timeout", timeout, 'bash', "-c", script],
        "Env": env,
        "AttachStdout": True,
        "AttachStderr": True,
        "Tty": False,  # Do not open terminal | 不打开终端
        "OpenStdin": False,  # Do not open stdin for the container | 不对容器开启标准输入
        "HostConfig": {
            "AutoRemove": isAutoRemove,  # Auto-remove container after exit | 容器退出后自动删除
            "NetworkMode": "bridge",
            "Memory": Conf.docker.create.memory,
            "CpuQuota": Conf.docker.create.cpuQuota,
        },
    }
    if binds:
        config["HostConfig"]["Binds"] = binds
    return config


class SetHeartRate(ParseHandler, CrosHandler):
    async def get(self):
        WorkflowType = 'BPM'
        Container = None
        IsAutoRemove = True
        try:
            param = self.parse_args(schemas.SetHeartRate)
            mac = param.mac
            iso8601 = param.iso8601
            bpm = param.bpm
            beats = param.beats
            if IsLocalhost:
                mac = "0"  # For easy testing

            # Store or update user | 存储或更新用户
            myUser = Users.get(mac)
            if myUser is None:
                myUser = User(mac.replace(':', ''), mac)
                Users[mac] = myUser

            # Extract timezone from iso8601, then get the current server iso8601 time using this timezone | 从 iso8601 中提取时区，再用此时区获取当前服务器的 iso8601 时间
            dt = datetime.fromisoformat(iso8601)
            tz = dt.tzinfo
            now_iso = datetime.now(tz).isoformat()

            msg_info = f"SetHeartRate: mac={mac}, iso8601={iso8601}, bpm={bpm}, beats={beats}"
            print(msg_info)

            self.set_header("Content-Type", "application/json")
            self.write(json.dumps({"ok": True}))
            await self.flush()

            self.finish()  # Close the connection in advance

            # For easy testing
            # myUser.wants = 'If my heart rate exceeds 60 beats per minute, send an email to my primary contact. Subject is "I Love You". Body is a romantic love poem based on my heart rate.'
            if not myUser.wants:
                return
            # Trigger workflow | 触发工作流
            docker_url = None
            if IsLocalhost:
                docker_url = Conf.docker.restUrl
            async with aiodocker.Docker(url=docker_url) as docker:
                # 1a. Check if workflow exists | 检查工作流是否存在
                workflow = schemas.WORKFLOWS.get(WorkflowType)
                if not workflow:
                    raise Exception('NO_WORKFLOW')
                # 1b. Check if image exists | 检查镜像是否存在
                try:
                    image = workflow['image']
                    container_name = f"wf_{WorkflowType}_{myUser.uid}_{'test' if IsLocalhost else int(time.time())}"
                    await docker.images.inspect(image)
                except DockerError as e:
                    raise HTTPError('NO_IAMGE', e.message, e.status)
                # 2. Get parameters | 获取参数
                system_prompt = ''
                user_prompt = myUser.wants
                user_data = f'BMP: {bpm}\nbeats: {beats}\ntouch_at: {now_iso}'
                docker_path = workflow["docker_path"]
                user_ws_path = Path(f"{Conf.workspace_base}/{myUser.uid}")
                bind_files: list[str] = []
                at_files = ''
                # 3. Update template parameters | 更新模板参数
                vars = dict()
                vars["user_lang"] = Language.get(self.request.headers.get('Accept-Language', 'en-US').split(',')[0]).language_name()
                vars["now"] = now_iso
                vars["access_token"] = myUser.access_token
                vars["primary_contact"] = 'aminlab@qq.com'
                vars["data"] = user_data
                vars["prompt"] = user_prompt
                vars["resolution"] = param.resolution
                vars["docker_path"] = docker_path
                vars["timestamp"] = int(time.time())
                SPT = workflow["system_prompt"]  # system prompt template
                UPT = workflow["user_prompt"]  # user prompt template
                if SPT:
                    system_prompt = ReplacePlaceholder(SPT.format_map(vars), vars)
                if UPT:
                    user_prompt = ReplacePlaceholder(UPT.format_map(vars), vars)
                # 4a. Map user workspace directory | 映射用户工作目录
                if user_ws_path:
                    if IsLocalhost:
                        print(f'user workspace: {user_ws_path} - note: you MUST create this folder on the server before testing')
                    else:
                        user_ws_path.mkdir(parents=True, exist_ok=True, mode=0o777)
                    bind_files.append(f"{user_ws_path.as_posix()}:{docker_path}")
                # 4b. Map log files | 映射日志文件
                bind_files.append(f"{Conf.logs_base}:/logs")
                # 5. Start container instance | 启动实例
                UID = 1000  # Ubuntu user | ubuntu 用户
                WF_CMD = workflow['docker_cmd'] or ''
                PROVIDER = Conf.workflow.provider
                MODEL = Conf.workflow.model
                API_KEY = Conf.workflow.apiKey or os.getenv('MODEL_API_KEY1') or ''
                if not PROVIDER or not MODEL or not API_KEY:
                    raise Exception('LACK_AGENT_PARAMS')
                SCRIPT = build_script(WF_CMD, system_prompt, user_prompt, PROVIDER, MODEL, at_files)
                ENV = [
                    'MODEL_API_KEY=' + API_KEY,
                ]
                bind_files.append(f'{Conf.pi_base}/extensions/tools.ts:/my/.pi/extensions/tools.ts:ro')
                bind_files.append(f'{Conf.pi_base}/agent/models.json:/home/ubuntu/.pi/agent/models.json')
                if IsLocalhost:
                    print(SCRIPT)
                config = build_docker_config(UID, image, '300s', SCRIPT, ENV, IsAutoRemove, bind_files)
                Container = await docker.containers.create(config=config, name=container_name)
                await Container.start()
                Workflows[container_name] = Container
                # 6. Print the agent's streaming output | 打印智能体的流输出
                await agent_print(Container, myUser.uid)
        except HTTPError as e:
            self.set_header("Content-Type", "application/json")
            self.set_status(e.status_code)
            self.write(e.to_json())
        except Exception as e:
            logging.exception(f"SetHeartRate GET failed: {e}")
            self.set_header("Content-Type", "application/json")
            self.set_status(500)
            self.write(json.dumps({"error": str(e)}))
        finally:
            if Container is not None:
                # Remove container reference | 移除容器引用
                Workflows.pop(container_name, None)
                # Force delete container | 强制关闭容器
                if IsAutoRemove == False:
                    try:
                        await Container.delete(force=True)
                    except Exception as e:
                        logging.warning(f"container delete failed: {e}")


class SetWantsAudio(ParseHandler, CrosHandler):
    async def post(self):
        try:
            wav_data = self.request.body
            if not wav_data:
                raise HTTPError('NO_AUDIO')
            param = self.parse_args(schemas.SetWantsAudio)
            mac = param.mac
            if IsLocalhost:
                mac = "0"  # For easy testing

            # Store or update user | 存储或更新用户
            myUser = Users.get(mac)
            if myUser is None:
                myUser = User(mac.replace(':', ''), mac)
                Users[mac] = myUser

            # Call model to transcribe audio | 调用模型转录音频
            httpClient = httpx.AsyncClient()
            if Conf.audio.provider == 'volcengine':
                wav_base64 = base64.b64encode(wav_data).decode('utf-8')
                headers = {
                    "X-Api-Key": Conf.audio.apiKey,
                    "X-Api-Resource-Id": Conf.audio.model,
                    "X-Api-Request-Id": str(uuid.uuid4()),
                    "X-Api-Sequence": "-1",
                }
                payload = {
                    "user": {"uid": Conf.audio.apiKey},
                    "audio": {"data": wav_base64},
                    "request": {"model_name": "bigmodel"},
                }
                resp = await httpClient.post("https://openspeech.bytedance.com/api/v3/auc/bigmodel/recognize/flash", json=payload, headers=headers)
                if resp.status_code != 200:
                    raise Exception(resp.text)
                body = resp.json()
                result_text = body['result']['text']
            elif Conf.audio.provider == 'openai':
                headers = {
                    "Authorization": f"Bearer {Conf.audio.apiKey}"
                }
                resp = await httpClient.post(
                    "https://api.openai.com/v1/audio/transcriptions",
                    data={"model": Conf.audio.model},
                    files={"file": ("audio.wav", wav_data, "audio/wav")},
                    headers=headers,
                )
                if resp.status_code != 200:
                    raise Exception(resp.text)
                body = resp.json()
                result_text = body['text']
            else:
                raise HTTPError('NO_PROVIDER')
            myUser.wants = result_text
            print('[Speech recognition results | 语音识别结果]', myUser.wants)

            self.set_header("Content-Type", "application/json")
            self.write(json.dumps({"ok": True}))
        except Exception as e:
            logging.exception(f"SetWantsAudio POST failed: {e}")
            self.set_header("Content-Type", "application/json")
            self.set_status(500)
            self.write(json.dumps({"error": str(e)}))


# Routes
ROUTES = [
    (r"/set-heart-rate", SetHeartRate),
    (r"/set-wants-audio", SetWantsAudio),
]

# test url
# http://localhost:3006/set-heart-rate?mac=98:3D:AE:E8:84:9C&bpm=72.0&beats=10&iso8601=2026-01-02T03%3A04%3A05%2B08%3A00
