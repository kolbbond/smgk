"""GUI tests: drive the real smgk-view binary (hidden window) against fixture or live sessions.

Run by ctest (label gui) with SMGK_VIEW set to the binary.
"""

import base64
import json
import os
import re
import shutil
import subprocess
import time
from pathlib import Path

import nbformat
import pytest
from nbformat import v4

VIEW = os.environ.get("SMGK_VIEW", str(Path(__file__).parents[1] / "build" / "smgk-view"))
FIXTURES = Path(__file__).parent / "fixtures"
RED_B64 = base64.b64encode((FIXTURES / "red4.png").read_bytes()).decode()


def make_session(root: Path, nb_path: str, generation: int = 1):
    (root / ".smgk").mkdir(exist_ok=True)
    (root / "notebooks").mkdir(exist_ok=True)
    # nothing listens on this port: the SUB socket just never receives
    (root / ".smgk" / "kernel.json").write_text(json.dumps({"transport": "tcp", "ip": "127.0.0.1", "iopub_port": 1, "key": ""}))
    st = {"generation": generation, "kernel_pid": 0, "recorder_pid": None,
          "connection_file": ".smgk/kernel.json", "notebook": nb_path, "python": "python3"}
    (root / ".smgk" / "state.json").write_text(json.dumps(st))


def summary(stdout: str):
    m = re.search(r"cells=(\d+) generation=(\d+)", stdout)
    assert m, f"no summary in output: {stdout!r}"
    return int(m.group(1)), int(m.group(2))


def run_view(root: Path, frames: int, *extra, timeout=60):
    return subprocess.run([VIEW, str(root), "--smg-hidden", "--smg-frames", str(frames), *extra],
                          capture_output=True, text=True, timeout=timeout)


def test_view_history(tmp_path):
    shutil.copy(FIXTURES / "history.ipynb", tmp_path / "h.ipynb")
    make_session(tmp_path, "h.ipynb")
    shot = tmp_path / "out.png"
    r = run_view(tmp_path, 30, "--smg-screenshot", str(shot))
    assert r.returncode == 0, r.stderr
    assert shot.exists()
    assert summary(r.stdout) == (3, 1)


def test_view_no_session(tmp_path):
    r = run_view(tmp_path, 10)
    assert r.returncode == 0, r.stderr
    assert summary(r.stdout) == (0, 0)


def test_view_restart(tmp_path):
    shutil.copy(FIXTURES / "history.ipynb", tmp_path / "h.ipynb")
    make_session(tmp_path, "h.ipynb")
    one = v4.new_notebook(cells=[v4.new_code_cell("x = 1", metadata={"smgk": {"msg_id": "Z", "kind": "cell"}})])
    nbformat.write(one, str(tmp_path / "notebooks" / "second.ipynb"))

    proc = subprocess.Popen([VIEW, str(tmp_path), "--smg-hidden", "--smg-frames", "200"], stdout=subprocess.PIPE, text=True)
    time.sleep(1.0)
    make_session(tmp_path, "notebooks/second.ipynb", generation=2)
    out, _ = proc.communicate(timeout=60)
    assert proc.returncode == 0
    assert summary(out) == (1, 2)


def test_view_many_images(tmp_path):
    cells = [v4.new_code_cell("", outputs=[v4.new_output("display_data", data={"image/png": RED_B64})],
                              metadata={"smgk": {"msg_id": f"I{i}", "kind": "push"}}) for i in range(300)]
    nbformat.write(v4.new_notebook(cells=cells), str(tmp_path / "many.ipynb"))
    make_session(tmp_path, "many.ipynb")
    r = run_view(tmp_path, 60)
    assert r.returncode == 0, r.stderr
    assert summary(r.stdout) == (300, 1)


@pytest.mark.skipif(shutil.which("smgk") is None, reason="smgk CLI not installed")
def test_view_live(tmp_path):
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    subprocess.run(["smgk", "start", "--dir", str(tmp_path)], check=True, capture_output=True)
    try:
        proc = subprocess.Popen([VIEW, str(tmp_path), "--smg-hidden", "--smg-frames", "240"], stdout=subprocess.PIPE, text=True)
        time.sleep(1.0)
        subprocess.run(["smgk", "run", "-c", "print('live')"], cwd=tmp_path, check=True, capture_output=True)
        subprocess.run(["smgk", "note", "hello"], cwd=tmp_path, check=True, capture_output=True)
        out, _ = proc.communicate(timeout=60)
        assert proc.returncode == 0
        assert summary(out) == (2, 1)
    finally:
        subprocess.run(["smgk", "stop", "--dir", str(tmp_path)], capture_output=True)
