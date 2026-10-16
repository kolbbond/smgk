"""Launch smgk-view for a project, at most one per project."""

import os
import shutil
import subprocess
from pathlib import Path
from typing import List

from .kernel import SmgkError, alive, proc_start
from .project import STATE_DIR


def launch(root: Path, extra: List[str]) -> int:
    root = Path(root).resolve()
    sd = root / STATE_DIR
    pidfile = sd / "view.pid"
    try:
        pid, start = (int(x) for x in pidfile.read_text().split())
        if alive(pid, start):
            return pid
    except (OSError, ValueError):
        pass

    exe = os.environ.get("SMGK_VIEW") or shutil.which("smgk-view")
    if not exe or not Path(exe).is_file():
        raise SmgkError(f"smgk: smgk-view not found (looked for {exe or 'smgk-view on PATH'}); run make install in smgk")
    sd.mkdir(exist_ok=True)
    with open(sd / "view.log", "ab") as log:
        proc = subprocess.Popen([exe, str(root), *extra], cwd=root, stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                start_new_session=True)
    pidfile.write_text(f"{proc.pid} {proc_start(proc.pid)}")
    return proc.pid
