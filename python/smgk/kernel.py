"""Kernel lifecycle: one detached ipykernel per project, described by .smgk/state.json."""

import json
import os
import signal
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from typing import Optional

from jupyter_client import BlockingKernelClient
from jupyter_client.connect import write_connection_file
from jupyter_client.session import Session

from .project import STATE_DIR, find_state_dir


class SmgkError(SystemExit):
    """Exit with a user-facing message; the CLI prints `msg` to stderr."""

    def __init__(self, msg: str, code: int = 1):
        super().__init__(code)
        self.msg = msg


class NoKernel(SmgkError):
    def __init__(self, start: Path):
        super().__init__(f"smgk: no kernel for {start} (run: smgk start)")


@dataclass
class State:
    generation: int
    kernel_pid: int
    recorder_pid: Optional[int]
    connection_file: str
    notebook: str
    python: str

    @classmethod
    def load(cls, state_dir: Path) -> Optional["State"]:
        try:
            return cls(**json.loads((state_dir / "state.json").read_text()))
        except (OSError, ValueError, TypeError):
            return None

    def save(self, state_dir: Path) -> None:
        tmp = state_dir / "state.json.tmp"
        tmp.write_text(json.dumps(asdict(self), indent=1))
        os.replace(tmp, state_dir / "state.json")


def kernel_python() -> str:
    venv = os.environ.get("VIRTUAL_ENV")
    return str(Path(venv) / "bin" / "python") if venv else sys.executable


def pid_alive(pid: Optional[int]) -> bool:
    if not pid:
        return False
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    # a zombie child of this process still "exists"; reap it if it's ours
    try:
        return os.waitpid(pid, os.WNOHANG) == (0, 0)
    except ChildProcessError:
        return True


def _client(root: Path, st: State, username: Optional[str] = None) -> BlockingKernelClient:
    kc = BlockingKernelClient(connection_file=str(root / st.connection_file))
    kc.load_connection_file()
    if username:
        kc.session = Session(key=kc.session.key, username=username)
    # no heartbeat: its thread races stop_channels() and liveness is checked by pid
    kc.start_channels(hb=False)
    return kc


def _responds(root: Path, st: State, timeout: float = 5) -> bool:
    kc = _client(root, st)
    try:
        kc.wait_for_ready(timeout=timeout)
        return True
    except RuntimeError:
        return False
    finally:
        kc.stop_channels()


def _notebook_name(root: Path, avoid: Optional[str]) -> str:
    while True:
        name = f"notebooks/{datetime.now():%Y-%m-%d-%H%M%S}.ipynb"
        if name != avoid and not (root / name).exists():
            return name
        time.sleep(0.2)


def start(root: Path, generation: int = 1, avoid_notebook: Optional[str] = None) -> State:
    root = Path(root).resolve()
    sd = root / STATE_DIR
    st = State.load(sd)
    if st and pid_alive(st.kernel_pid) and _responds(root, st):
        return st

    py = kernel_python()
    if subprocess.run([py, "-c", "import ipykernel"], capture_output=True).returncode != 0:
        raise SmgkError(f"ipykernel is not installed in {py}; run: {py} -m pip install ipykernel", code=2)

    sd.mkdir(exist_ok=True)
    (root / "notebooks").mkdir(exist_ok=True)
    conn = sd / "kernel.json"
    conn.unlink(missing_ok=True)
    write_connection_file(str(conn), ip="127.0.0.1")
    env = {k: v for k, v in os.environ.items() if k != "MPLBACKEND"}  # inline backend → PNG display_data
    with open(sd / "kernel.log", "ab") as log:
        proc = subprocess.Popen(
            [py, "-m", "ipykernel_launcher", "-f", str(conn)],
            cwd=root, env=env, stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True,
        )
    st = State(generation, proc.pid, None, f"{STATE_DIR}/kernel.json", _notebook_name(root, avoid_notebook), py)
    if not _responds(root, st, timeout=30):
        proc.kill()
        raise SmgkError(f"smgk: kernel failed to start, see {sd / 'kernel.log'}")
    st.save(sd)
    return st


def _terminate(pid: Optional[int], timeout: float = 5) -> None:
    if not pid_alive(pid):
        return
    os.kill(pid, signal.SIGTERM)
    deadline = time.time() + timeout
    while pid_alive(pid) and time.time() < deadline:
        time.sleep(0.05)
    if pid_alive(pid):
        os.kill(pid, signal.SIGKILL)


def stop(root: Path) -> Optional[State]:
    root = Path(root).resolve()
    sd = root / STATE_DIR
    st = State.load(sd)
    if not st:
        return None
    if pid_alive(st.kernel_pid):
        kc = _client(root, st)
        kc.shutdown()
        # closing channels right away drops the queued request; wait for the exit first
        deadline = time.time() + 5
        while pid_alive(st.kernel_pid) and time.time() < deadline:
            time.sleep(0.05)
        kc.stop_channels()
        _terminate(st.kernel_pid)
    _terminate(st.recorder_pid)
    (sd / "state.json").unlink(missing_ok=True)
    return st


def restart(root: Path) -> State:
    old = stop(root)
    gen = old.generation + 1 if old else 1
    return start(root, generation=gen, avoid_notebook=old.notebook if old else None)


def status(root: Path) -> Optional[State]:
    st = State.load(Path(root).resolve() / STATE_DIR)
    return st if st and pid_alive(st.kernel_pid) else None


def connect(start: Path, username: Optional[str] = None) -> BlockingKernelClient:
    sd = find_state_dir(start)
    st = State.load(sd) if sd else None
    if not st or not pid_alive(st.kernel_pid):
        raise NoKernel(Path(start).resolve())
    return _client(sd.parent, st, username)
