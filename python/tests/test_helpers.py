import base64
import subprocess
import time

from smgk import helpers, kernel

from msgs import PNG_4x4_RED, wait_cells


def test_push_png(project, tmp_path_factory):
    img = tmp_path_factory.mktemp("img") / "plot.png"
    img.write_bytes(PNG_4x4_RED)
    st = kernel.start(project)
    helpers.push(project, img)
    c = wait_cells(project / st.notebook, 1).cells[0]
    assert c.cell_type == "code" and c.source == ""
    assert c.metadata.smgk.kind == "push"
    assert c.outputs[0].data["image/png"].replace("\n", "") == base64.b64encode(PNG_4x4_RED).decode()


def test_note_markdown(project):
    st = kernel.start(project)
    helpers.note(project, "**fit converged**")
    c = wait_cells(project / st.notebook, 1).cells[0]
    assert c.cell_type == "markdown" and c.source == "**fit converged**"


def test_run_visible(project):
    st = kernel.start(project)
    helpers.run(project, "y = 2 * 21\nprint(y)")
    c = wait_cells(project / st.notebook, 1).cells[0]
    assert c.source == "y = 2 * 21\nprint(y)" and c.metadata.smgk.kind == "run"
    assert c.outputs[0].text == "42\n"


def test_helpers_leave_namespace_clean(project, tmp_path_factory):
    img = tmp_path_factory.mktemp("img") / "p.png"
    img.write_bytes(PNG_4x4_RED)
    kernel.start(project)
    helpers.push(project, img)
    helpers.note(project, "n")
    kc = kernel.connect(project)
    try:
        reply = kc.execute_interactive("", user_expressions={"names": "sorted(k for k in globals() if 'smgk' in k)"}, timeout=10)
    finally:
        kc.stop_channels()
    assert reply["content"]["user_expressions"]["names"]["data"]["text/plain"] == "[]"


def _cli(*args, cwd):
    t = time.time()
    r = subprocess.run(["smgk", *args], cwd=cwd, capture_output=True, text=True, timeout=10)
    return r, time.time() - t


def test_push_no_kernel(tmp_path):
    img = tmp_path / "a.png"
    img.write_bytes(PNG_4x4_RED)
    r, dt = _cli("push", str(img), cwd=tmp_path)
    assert r.returncode == 1
    assert r.stderr.strip() == f"smgk: no kernel for {tmp_path.resolve()} (run: smgk start)"
    assert dt < 2


def test_push_missing_file(tmp_path):
    r, _ = _cli("push", "nope.png", cwd=tmp_path)
    assert r.returncode == 1
    assert r.stderr.strip() == f"smgk: no such file: {tmp_path.resolve() / 'nope.png'}"
