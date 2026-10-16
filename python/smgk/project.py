"""Project discovery: one kernel per git root (else the start dir), state in <root>/.smgk/."""

import subprocess
from pathlib import Path
from typing import Optional

STATE_DIR = ".smgk"


def git_root(start: Path) -> Optional[Path]:
    r = subprocess.run(["git", "-C", str(start), "rev-parse", "--show-toplevel"], capture_output=True, text=True)
    return Path(r.stdout.strip()) if r.returncode == 0 else None


def find_root(start: Path) -> Path:
    start = Path(start).resolve()
    return git_root(start) or start


def find_state_dir(start: Path) -> Optional[Path]:
    """Nearest .smgk/ at or above start, never above the enclosing git root (a stray
    .smgk further up must not capture another repo's pushes)."""
    start = Path(start).resolve()
    stop = git_root(start)
    for d in [start, *start.parents]:
        if (d / STATE_DIR).is_dir():
            return d / STATE_DIR
        if d == stop:
            return None
    return None


def resolve_root(start: Path) -> Path:
    """Project root for lifecycle commands: an existing session wins, so a subdirectory of a
    non-git project reaches the kernel started at its top instead of starting another."""
    sd = find_state_dir(start)
    return sd.parent if sd else find_root(start)
