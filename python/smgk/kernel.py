"""Kernel lifecycle: one detached ipykernel per project, described by .smgk/state.json."""

import fcntl
import json
import os
import signal
import subprocess
import sys
import time
from contextlib import contextmanager
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterator, Optional

from jupyter_client.blocking.client import BlockingKernelClient
from jupyter_client.connect import write_connection_file
from jupyter_client.session import Session

from .project import STATE_DIR, find_state_dir


class SmgkError(SystemExit):
    """Exit with a user-facing message; the CLI prints `msg` to stderr."""

    def __init__(self, msg: str, code: int = 1):
        super().__init__(code)
        self.msg = msg
        self.status = code  # SystemExit.code is typed str | int | None


class NoKernel(SmgkError):
    def __init__(self, start: Path):
        super().__init__(f"smgk: no kernel for {start} (run: smgk start)")


def proc_start(pid: Optional[int]) -> Optional[int]:
    """Start time (clock ticks since boot) of a live, non-zombie pid; None otherwise."""
    if not pid:
        return None
    try:
        stat = Path(f"/proc/{pid}/stat").read_text()
    except OSError:
        return None
    fields = stat[stat.rindex(")") + 2:].split()  # comm may contain spaces; fields resume at 3
    if fields[0] == "Z":
        try:
            os.waitpid(pid, os.WNOHANG)  # reap it if it's our child
        except ChildProcessError:
            pass
        return None
    return int(fields[19])  # field 22: starttime


def alive(pid: Optional[int], start: Optional[int]) -> bool:
    # pids are reused (fast, after a WSL restart): identity is pid plus start time
    return start is not None and proc_start(pid) == start


@dataclass
class State:
    generation: int
    kernel_pid: int
    recorder_pid: Optional[int]
    connection_file: str
    notebook: str
    python: str
    kernel_start: Optional[int] = None
    recorder_start: Optional[int] = None

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

    def kernel_alive(self) -> bool:
        return alive(self.kernel_pid, self.kernel_start)

    def recorder_alive(self) -> bool:
        return alive(self.recorder_pid, self.recorder_start)


@contextmanager
def _locked(sd: Path) -> Iterator[None]:
    # serializes start/stop/restart: nvim and an agent can both run `smgk start` at once
    sd.mkdir(parents=True, exist_ok=True)
    with open(sd / "lock", "w") as f:
        fcntl.flock(f, fcntl.LOCK_EX)
        yield


def kernel_python() -> str:
    venv = os.environ.get("VIRTUAL_ENV")
    return str(Path(venv) / "bin" / "python") if venv else sys.executable


def _client(root: Path, st: State, username: Optional[str] = None) -> BlockingKernelClient:
    kc = BlockingKernelClient(connection_file=str(root / st.connection_file))
    kc.load_connection_file()
    if username:
        kc.session = Session(key=kc.session.key, username=username)
    # no heartbeat: its thread races stop_channels() and liveness is checked by pid identity
    kc.start_channels(hb=False)
    return kc


def _responds(root: Path, st: State, timeout: float) -> bool:
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


def _terminate(pid: Optional[int], start: Optional[int], timeout: float = 5) -> None:
    # identity is rechecked before every signal: never touch a process we didn't start
    if pid is None or not alive(pid, start):
        return
    os.kill(pid, signal.SIGTERM)
    deadline = time.time() + timeout
    while alive(pid, start) and time.time() < deadline:
        time.sleep(0.05)
    if alive(pid, start):
        os.kill(pid, signal.SIGKILL)


def _start_recorder(root: Path, timeout: float = 10) -> subprocess.Popen:
    sd = root / STATE_DIR
    ready = sd / "recorder.ready"
    ready.unlink(missing_ok=True)
    with open(sd / "recorder.log", "ab") as log:
        proc = subprocess.Popen(
            [sys.executable, "-m", "smgk.recorder", str(root)],
            cwd=root, stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True,
        )
    deadline = time.time() + timeout
    while not ready.exists():
        if time.time() > deadline or proc.poll() is not None:
            proc.kill()
            raise SmgkError(f"smgk: recorder failed to start, see {sd / 'recorder.log'}")
        time.sleep(0.05)
    return proc


def _start(root: Path, generation: Optional[int], avoid_notebook: Optional[str]) -> State:
    sd = root / STATE_DIR
    old = State.load(sd)
    # alive by identity, not by a shell round-trip: a busy kernel answers nothing until its cell ends
    if old and old.kernel_alive():
        if not old.recorder_alive():
            rec = _start_recorder(root)
            old.recorder_pid, old.recorder_start = rec.pid, proc_start(rec.pid)
            old.save(sd)
        return old
    if old:
        _terminate(old.recorder_pid, old.recorder_start)
    if generation is None:
        # a crashed kernel's successor gets a new generation so open viewers reconnect
        generation = old.generation + 1 if old else 1

    py = kernel_python()
    if subprocess.run([py, "-c", "import ipykernel"], capture_output=True).returncode != 0:
        raise SmgkError(f"ipykernel is not installed in {py}; run: {py} -m pip install ipykernel", code=2)

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
    st = State(generation, proc.pid, None, f"{STATE_DIR}/kernel.json", _notebook_name(root, avoid_notebook or (old and old.notebook)),
               py, kernel_start=proc_start(proc.pid))
    if not _responds(root, st, timeout=30):
        proc.kill()
        raise SmgkError(f"smgk: kernel failed to start, see {sd / 'kernel.log'}")
    st.save(sd)
    rec = _start_recorder(root)
    st.recorder_pid, st.recorder_start = rec.pid, proc_start(rec.pid)
    st.save(sd)
    return st


def _stop(root: Path) -> Optional[State]:
    sd = root / STATE_DIR
    st = State.load(sd)
    if not st:
        return None
    if st.kernel_alive():
        kc = _client(root, st)
        kc.shutdown()
        # closing channels right away drops the queued request; wait for the exit first
        deadline = time.time() + 5
        while st.kernel_alive() and time.time() < deadline:
            time.sleep(0.05)
        kc.stop_channels()
        _terminate(st.kernel_pid, st.kernel_start)
    _terminate(st.recorder_pid, st.recorder_start)
    (sd / "state.json").unlink(missing_ok=True)
    return st


def start(root: Path) -> State:
    root = Path(root).resolve()
    with _locked(root / STATE_DIR):
        return _start(root, None, None)


def stop(root: Path) -> Optional[State]:
    root = Path(root).resolve()
    if not (root / STATE_DIR).is_dir():
        return None
    with _locked(root / STATE_DIR):
        return _stop(root)


def restart(root: Path) -> State:
    root = Path(root).resolve()
    with _locked(root / STATE_DIR):
        old = _stop(root)
        return _start(root, old.generation + 1 if old else None, old.notebook if old else None)


def status(root: Path) -> Optional[State]:
    st = State.load(Path(root).resolve() / STATE_DIR)
    return st if st and st.kernel_alive() else None


def connect(start: Path, username: Optional[str] = None) -> BlockingKernelClient:
    sd = find_state_dir(start)
    st = State.load(sd) if sd else None
    if sd is None or st is None or not st.kernel_alive():
        raise NoKernel(Path(start).resolve())
    return _client(sd.parent, st, username)
