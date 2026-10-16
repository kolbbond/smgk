import re
import subprocess
import sys

import pytest

from smgk import kernel, project as proj


def test_find_root_git(project):
    sub = project / "a" / "b"
    sub.mkdir(parents=True)
    assert proj.find_root(sub) == project


def test_find_root_plain(tmp_path):
    assert proj.find_root(tmp_path) == tmp_path


def test_find_state_dir_walks_up(project):
    (project / ".smgk").mkdir()
    sub = project / "x" / "y"
    sub.mkdir(parents=True)
    assert proj.find_state_dir(sub) == project / ".smgk"


def test_start_creates_state(project):
    st = kernel.start(project)
    assert (project / ".smgk" / "kernel.json").exists()
    assert (project / ".smgk" / "state.json").exists()
    assert st.generation == 1
    assert re.fullmatch(r"notebooks/\d{4}-\d\d-\d\d-\d{6}\.ipynb", st.notebook)


def test_start_idempotent(project):
    a = kernel.start(project)
    b = kernel.start(project)
    assert a.kernel_pid == b.kernel_pid


def test_execute_roundtrip(project):
    kernel.start(project)
    kc = kernel.connect(project)
    try:
        reply = kc.execute_interactive("x = 41 + 1", user_expressions={"x": "x"}, timeout=10)
    finally:
        kc.stop_channels()
    assert reply["content"]["user_expressions"]["x"]["data"]["text/plain"] == "42"


def test_restart_bumps_generation(project):
    a = kernel.start(project)
    b = kernel.restart(project)
    assert b.generation == 2
    assert b.kernel_pid != a.kernel_pid
    assert b.notebook != a.notebook


def test_missing_ipykernel(project, monkeypatch, tmp_path_factory):
    venv = tmp_path_factory.mktemp("venv")
    subprocess.run([sys.executable, "-m", "venv", "--without-pip", str(venv)], check=True)
    monkeypatch.setenv("VIRTUAL_ENV", str(venv))
    with pytest.raises(SystemExit) as exc:
        kernel.start(project)
    assert exc.value.code == 2
    assert "-m pip install ipykernel" in exc.value.msg


def test_stop_is_graceful(project):
    import time

    kernel.start(project)
    t = time.time()
    kernel.stop(project)
    # graceful shutdown is ~0.2 s; the SIGTERM fallback only fires after 5 s
    assert time.time() - t < 2
    assert kernel.status(project) is None
