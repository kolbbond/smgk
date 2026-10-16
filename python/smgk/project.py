"""Project discovery: one kernel per git root (else the start dir), state in <root>/.smgk/."""

import subprocess
from pathlib import Path
from typing import Optional

STATE_DIR = ".smgk"


def find_root(start: Path) -> Path:
    start = Path(start).resolve()
    r = subprocess.run(["git", "-C", str(start), "rev-parse", "--show-toplevel"], capture_output=True, text=True)
    return Path(r.stdout.strip()) if r.returncode == 0 else start


def find_state_dir(start: Path) -> Optional[Path]:
    for d in [Path(start).resolve(), *Path(start).resolve().parents]:
        if (d / STATE_DIR).is_dir():
            return d / STATE_DIR
    return None
