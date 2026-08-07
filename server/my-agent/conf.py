import os
import re
import sys
import json
from typing import Literal


class conf_smtp_sender:
    username: str
    password: str
    from_address: str
    from_name: str


class conf_smtp:
    host: str
    port: int
    senders: dict[str, conf_smtp_sender]


class conf_docker_create:
    memory: int
    cpuQuota: int


class conf_docker:
    restUrl: str
    create: conf_docker_create


class conf_vps:
    host: str
    port: int
    username: str
    password: str


class conf_volcengine:
    apiKey: str
    resourceId: str


class conf_workflow:
    provider: Literal["deepseek", "openai"]
    model: Literal["deepseek-v4-pro", "deepseek-v4-flash", "gpt-5.6-luna", "gpt-5.6-sol", "gpt-5.6-terra"]
    apiKey: str


class conf_audio:
    provider:  Literal["volcengine", "openai"]
    model: Literal["volc.bigasr.auc_turbo", "whisper-1"]
    apiKey: str


class CONFIG:
    confName: str = ''
    port: int
    errLog: str
    outLog: str
    smtp: conf_smtp
    docker: conf_docker
    volcengine: conf_volcengine
    workflow: conf_workflow
    audio: conf_audio
    vps: conf_vps
    workspace_base: str
    logs_base: str
    pi_base: str


class DictClass(dict):
    __getattr__ = dict.__getitem__


Conf: CONFIG = CONFIG()
IsLocalhost = sys.platform in ('win32', 'darwin')


def _convert_any_to_cls(o):
    if isinstance(o, dict):
        dc = DictClass()
        for k, v in o.items():
            dc[k] = _convert_any_to_cls(v)
        return dc
    if isinstance(o, list):
        li = list()
        for v in o:
            li.append(_convert_any_to_cls(v))
        return li
    return o


def LoadConf(fileName):
    ins = None
    print(os.path.dirname('.'))
    if os.path.exists(fileName):
        with open(fileName, 'r', encoding='utf-8') as file:
            text = file.read()
            text = re.sub(r'\s+//.*?$', '', text, flags=re.MULTILINE)
            d = json.loads(text or '{}')
            ins = _convert_any_to_cls(d)
            Conf.__dict__.update(ins)
            Conf.confName = fileName
    else:
        raise Exception(f"Not found conf file:{fileName}")
    print('LoadConf', fileName)
    return ins
