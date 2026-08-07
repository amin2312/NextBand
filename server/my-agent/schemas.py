from pydantic import BaseModel, Field
from typing import Literal, Optional, TypedDict


class WORKFLOW(TypedDict):
    system_prompt: str
    user_prompt: str
    image: str
    docker_path: str
    docker_cmd: str


WORKFLOWS: dict[str, WORKFLOW] = {
    'BPM': {
        'image': 'pi_bpm',
        'system_prompt': '<INSTALLATIONS>\n$(prompts/installations.md)$\n</INSTALLATIONS>\n<WORKFLOW>\n$(prompts/bpm.md)$\n</WORKFLOW>\nNow: {now}\nUser language: {user_lang}\nUser access_token: {access_token}\nMy primary contact: {primary_contact}',
        'user_prompt': '<user_data>\n{data}\n</user_data>\n<user_wants>\n{prompt}\n</user_wants>\nUse the WORKFLOW to complete the task.',
        'docker_path': '/my',
        'docker_cmd': "rm -f /my/*;",
    }
}


class SetHeartRate(BaseModel):
    mac: str
    iso8601: str
    bpm: str
    beats: str
    resolution: Optional[str] = ''


class SetWantsAudio(BaseModel):
    mac: str


class SendAnEmail(BaseModel):
    access_token: str
    to_address: str
    subject: str
    body: str
