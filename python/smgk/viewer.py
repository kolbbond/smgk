"""Launch smgk-view for a project, at most one per project."""

import os
import shutil
import subprocess
from pathlib import Path
from typing import List

from .kernel import SmgkError, pid_alive
from .project import STATE_DIR


def launch(root: Path, extra: List[str]) -> int:
    root = Path(root).resolve()
    sd = root / STATE_DIR
    pidfile = sd / "view.pid"
    try:
        pid = int(pidfile.read_text())
        if pid_alive(pid):
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
    pidfile.write_text(str(proc.pid))
    return proc.pid
