import json
import re
from pathlib import Path
from typing import Any

from aiodocker.containers import DockerContainer
from limits.strategies import FixedWindowRateLimiter
from limits.storage import MemoryStorage
from limits import parse as limit_parse


def ReplacePlaceholder(text: str, vars: dict[str, Any]) -> str:
    def _file(match: re.Match[str]) -> str:
        filepath = match.group(1)
        if '..' in filepath or filepath.startswith('/'):  # Prevent path traversal
            return ''
        fullpath = Path(__file__).parent / filepath
        if not fullpath.is_file():
            return ''
        return fullpath.read_text(encoding="utf-8")

    def _var(match: re.Match[str]) -> str:
        key = match.group(1)
        return str(vars.get(key, match.group(0)))

    newone = re.sub(r"\$\((.+?)\)\$", _file, text)
    return re.sub(r"\{\{(\w+)\}\}", _var, newone)


_print_buf: dict = {}


def _delta_print(header: str, key, full: str) -> None:
    prev_len = _print_buf.get(key, 0)
    if prev_len == 0:
        print(header, end=' ', flush=True)
    delta = full[prev_len:]
    if delta:
        print(delta, end='', flush=True)
        _print_buf[key] = len(full)


def _print_stream_line(raw: str) -> None:
    try:
        item: dict = json.loads(raw)
    except Exception:
        print(raw)
        return

    STEP: str = item.get('type', '')
    MSG: dict = item.get('message') or {}
    ROLE: str = MSG.get('role', '')

    if STEP == 'FINISH':
        isError = MSG.get("isError", False)
        content = item.get('content', '')
        print(f'[finish] ok: {not isError}, {content}')
        return
    if STEP == 'session':
        print(f'[session] cwd: {item.get("cwd", "")}')
        return
    if STEP == 'agent_start':
        print('[agent] [start]')
        return
    if STEP == 'agent_end':
        messages = item.get('messages', [])
        last = messages[-1] if messages else {}
        print(f'[agent] [end] messages: {len(messages)}, stop reason: {last.get("stopReason", "")}')
        return
    if STEP == 'turn_start':
        print('[turn] [start]')
        return
    if STEP == 'turn_end':
        usage = MSG.get('usage', {})
        print(f'[turn] [end] input: {usage.get("input",0)}, output: {usage.get("output",0)}, stop reason: {MSG.get("stopReason","")}')
        return

    if ROLE == 'user':
        return
    if ROLE == 'toolResult':
        isError = MSG.get("isError", False)
        content = MSG.get('content', '')
        print(f'[{ROLE}] ok: {not isError}, {content}')
        return

    # Tool execution events
    if STEP in ('tool_execution_start', 'tool_execution_update', 'tool_execution_end'):
        tool = item.get('toolName', '')
        header = f'[{STEP}] [{tool}]'

        if STEP == 'tool_execution_start':
            print(f'{header} {item.get("args")}')
            _print_buf.clear()
        elif STEP == 'tool_execution_end':
            isError = item.get("isError", False)
            print(f'\n{header} ok: {not isError}, {item.get("result", "")}')
            _print_buf.clear()
        else:  # tool_execution_update
            _delta_print(header, STEP, f'{item.get("partialResult")}')
        return

    # Message events
    if STEP in ('message_start', 'message_update', 'message_end'):
        header = f'[{STEP}]'

        if ROLE == 'assistant':
            if STEP == 'message_start':
                print(f'{header} provider: {MSG.get("provider", "")}, model: {MSG.get("model", "")}')
                _print_buf.clear()
                return
            if STEP == 'message_end':
                usage = MSG.get('usage', {})
                print(f'\n{header} input: {usage.get("input",0)}, cache read: {usage.get("cacheRead",0)}, output: {usage.get("output",0)}, stop reason: {MSG.get("stopReason","")}')
                _print_buf.clear()
                return
            # message_update – stream thinking/text blocks with delta printing
            for i, block in enumerate(MSG.get('content', [])):
                bt = block.get('type', '')
                if bt in ('thinking', 'text'):
                    _delta_print(f'\n{header} [{bt}]', (bt, i),  block.get(bt, ''))
            return

        print(f'{header} {raw}')
        return

    print(f'[Unknown] {raw}')


_print_limiter = FixedWindowRateLimiter(MemoryStorage())
_print_rate = limit_parse("1/second")


async def agent_print(container: DockerContainer, uid: str) -> None:
    FIN = {"type": "FINISH", "isError": False, "content": "ok"}
    buffer = ''
    cached_line: str | None = None

    async for chunk in container.log(stdout=True, stderr=True, follow=True):
        if not chunk:
            continue

        # Accumulate and split into complete lines
        buffer += chunk
        while "\n" in buffer:
            line, sep, rest = buffer.partition('\n')
            if not sep:
                break
            buffer = rest

            # Filter: skip non-JSON, toolCall, and noisy message_start
            if not line.startswith('{"type":'):
                continue
            if line.startswith('{"type":"toolCall"'):
                continue
            if '"message_start"' in line and ('"role":"user"' in line or '"role":"toolResult"' in line):
                continue

            # Rate-limit assistantMessageEvent (keep only latest)
            if '"assistantMessageEvent"' in line:
                if not _print_limiter.hit(_print_rate, uid):
                    # Over quota: stash line, will flush when next event comes
                    cached_line = line
                    continue
                # Within quota: strip the wrapper key before printing
                try:
                    item: dict = json.loads(line)
                    item.pop('assistantMessageEvent', None)
                    line = json.dumps(item)
                except Exception:
                    print('ERROR STREAM LINE', line)
                    pass
                cached_line = None

            # Capture error info from agent_end
            elif '"agent_end"' in line:
                try:
                    item: dict = json.loads(line)
                    messages = item.get('messages')
                    if messages:
                        last: dict = messages[-1]
                        if last.get('stopReason') == 'error':
                            FIN['isError'] = False
                            FIN['content'] = last.get('errorMessage')
                except Exception:
                    pass

            # Flush any previously-cached rate-limited line before this one
            if cached_line:
                _print_stream_line(cached_line)
                cached_line = None
            _print_stream_line(line)

    # Stream ended: flush leftovers
    if cached_line:
        _print_stream_line(cached_line)
        cached_line = None
    buffer = buffer.strip()
    if buffer:
        _print_stream_line(buffer)
    # Always emit a synthetic FINISH marker
    _print_stream_line(json.dumps(FIN))
