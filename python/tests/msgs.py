"""Builders for IOPub message dicts shaped like jupyter_client's get_iopub_msg()."""

from pathlib import Path

PNG_4x4_RED = (Path(__file__).parents[2] / "test" / "fixtures" / "red4.png").read_bytes()


def msg(msg_type, parent, content, username="user", metadata=None):
    return {
        "msg_type": msg_type,
        "header": {"msg_type": msg_type, "msg_id": f"{msg_type}-{parent}"},
        "parent_header": {"msg_id": parent, "username": username},
        "metadata": metadata or {},
        "content": content,
    }


def execute_input(parent, code, count=1, username="user"):
    return msg("execute_input", parent, {"code": code, "execution_count": count}, username)


def stream(parent, text, name="stdout"):
    return msg("stream", parent, {"name": name, "text": text})


def display(parent, data, md=None):
    return msg("display_data", parent, {"data": data, "metadata": md or {}, "transient": {}})


def idle(parent):
    return msg("status", parent, {"execution_state": "idle"})
