import base64
import nbformat

from smgk import kernel
from smgk.recorder import NotebookBuilder

from msgs import PNG_4x4_RED, display, execute_input, idle, stream, wait_cells


def build(*msgs):
    b = NotebookBuilder()
    for m in msgs:
        b.apply(m)
    return b.notebook()


def test_builder_groups_outputs():
    nb = build(execute_input("A", "print(1)"), stream("A", "1\n"), idle("A"))
    assert len(nb.cells) == 1
    c = nb.cells[0]
    assert c.cell_type == "code" and c.source == "print(1)" and c.execution_count == 1
    assert [o.text for o in c.outputs] == ["1\n"]
    assert c.metadata.smgk == {"msg_id": "A", "kind": "cell"}


def test_builder_run_kind():
    nb = build(execute_input("R", "x = 1", username="smgk"))
    assert nb.cells[0].metadata.smgk.kind == "run"


def test_builder_note_is_markdown():
    nb = build(display("N", {"text/markdown": "# hi", "text/plain": "# hi"}, {"smgk": {"kind": "note"}}))
    assert len(nb.cells) == 1
    c = nb.cells[0]
    assert c.cell_type == "markdown" and c.source == "# hi"
    assert c.metadata.smgk == {"msg_id": "N", "kind": "note"}


def test_builder_push_output_only():
    b64 = base64.b64encode(PNG_4x4_RED).decode()
    nb = build(display("P", {"image/png": b64, "text/plain": "<Image>"}, {"smgk": {"kind": "push"}}))
    c = nb.cells[0]
    assert c.cell_type == "code" and c.source == ""
    assert c.metadata.smgk.kind == "push"
    assert c.outputs[0].output_type == "display_data" and c.outputs[0].data["image/png"] == b64


def test_builder_stream_merge():
    nb = build(execute_input("A", "x"), stream("A", "a\n"), stream("A", "b\n"))
    assert [o.text for o in nb.cells[0].outputs] == ["a\nb\n"]


def test_builder_resumes_existing_notebook():
    first = build(execute_input("A", "x"), stream("A", "a\n"))
    b = NotebookBuilder(first)
    b.apply(stream("A", "b\n"))
    b.apply(execute_input("B", "y", 2))
    nb = b.notebook()
    assert [c.metadata.smgk.msg_id for c in nb.cells] == ["A", "B"]
    assert nb.cells[0].outputs[0].text == "a\nb\n"


def test_recorder_live(project):
    st = kernel.start(project)
    assert kernel.pid_alive(st.recorder_pid)
    kc = kernel.connect(project)
    try:
        kc.execute_interactive('print("hi")', timeout=10)
        kc.execute_interactive("import matplotlib.pyplot as plt\nplt.plot([1, 3, 2])\nplt.show()", timeout=30)
    finally:
        kc.stop_channels()
    nb = wait_cells(project / st.notebook, 2)
    nbformat.validate(nb)
    assert nb.cells[0].source == 'print("hi")' and nb.cells[0].outputs[0].text == "hi\n"
    assert any("image/png" in o.get("data", {}) for o in nb.cells[1].outputs)


def test_stop_ends_recorder(project):
    st = kernel.start(project)
    kernel.stop(project)
    assert not kernel.pid_alive(st.recorder_pid)
