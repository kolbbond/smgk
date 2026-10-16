"""Regressions from the final review: pid identity, busy kernels, crashes, races, project resolution."""

import json
import os
import signal
import subprocess
import time

import pytest

from smgk import helpers, kernel

from msgs import wait_cells


def test_stale_pid_is_not_signalled(tmp_path):
    # after a reboot the recorded pids can belong to anything (here: an innocent sleep)
    innocent = subprocess.Popen(["sleep", "300"])
    try:
        (tmp_path / ".smgk").mkdir()
        (tmp_path / ".smgk" / "state.json").write_text(json.dumps({
            "generation": 1, "kernel_pid": innocent.pid, "recorder_pid": innocent.pid,
            "connection_file": ".smgk/kernel.json", "notebook": "notebooks/x.ipynb", "python": "python3"}))
        t = time.time()
        kernel.stop(tmp_path)
        assert time.time() - t < 1
        assert innocent.poll() is None, "stop() killed a process it did not start"
        assert kernel.status(tmp_path) is None
        with pytest.raises(kernel.NoKernel):
            kernel.connect(tmp_path)
    finally:
        innocent.kill()


def test_start_while_busy_keeps_kernel(project):
    a = kernel.start(project)
    kc = kernel.connect(project)
    try:
        kc.execute("import time; time.sleep(8)")
        time.sleep(0.5)
        t = time.time()
        b = kernel.start(project)
        assert b.kernel_pid == a.kernel_pid
        assert time.time() - t < 3
    finally:
        kc.stop_channels()


def test_start_after_crash_bumps_generation(project):
    a = kernel.start(project)
    os.kill(a.kernel_pid, signal.SIGKILL)
    deadline = time.time() + 5
    while kernel.status(project) and time.time() < deadline:
        time.sleep(0.05)
    b = kernel.start(project)
    assert b.generation == a.generation + 1
    assert b.kernel_pid != a.kernel_pid


def test_concurrent_start(project):
    procs = [subprocess.Popen(["smgk", "start", "--dir", str(project)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
             for _ in range(2)]
    outs = [p.communicate(timeout=60) for p in procs]
    assert [p.returncode for p in procs] == [0, 0], outs
    assert outs[0][0] == outs[1][0]
    assert kernel.status(project) is not None


def test_run_has_no_deadline(project, monkeypatch):
    monkeypatch.setattr(helpers, "QUEUE_TIMEOUT", 1)
    kernel.start(project)
    assert helpers.run(project, "import time; time.sleep(2)\nprint('ok')")


def test_note_while_busy_is_queued(project, monkeypatch, capsys):
    monkeypatch.setattr(helpers, "QUEUE_TIMEOUT", 1)
    st = kernel.start(project)
    kc = kernel.connect(project)
    try:
        kc.execute("import time; time.sleep(3)")
        time.sleep(0.3)
        helpers.note(project, "queued note")
    finally:
        kc.stop_channels()
    assert "queued" in capsys.readouterr().err
    nb = wait_cells(project / st.notebook, 2)
    assert nb.cells[-1].source == "queued note"


def _smgk(*args):
    return subprocess.run(["smgk", *args], capture_output=True, text=True, timeout=60)


def test_lifecycle_from_subdir_non_git(tmp_path):
    sub = tmp_path / "sub"
    sub.mkdir()
    try:
        first = _smgk("start", "--dir", str(tmp_path))
        assert first.returncode == 0
        assert _smgk("status", "--dir", str(sub)).returncode == 0
        again = _smgk("start", "--dir", str(sub))
        assert again.stdout == first.stdout
        assert not (sub / ".smgk").exists()
    finally:
        kernel.stop(tmp_path)


def test_lookup_stops_at_git_root(tmp_path):
    inner = tmp_path / "repo"
    subprocess.run(["git", "init", "-q", str(inner)], check=True)
    try:
        assert _smgk("start", "--dir", str(tmp_path)).returncode == 0
        with pytest.raises(kernel.NoKernel):
            kernel.connect(inner)
    finally:
        kernel.stop(tmp_path)
