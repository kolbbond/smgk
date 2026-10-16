"""Recorder: sole writer of the session .ipynb, built from the kernel's IOPub broadcast.

Run as `python -m smgk.recorder <root>`; started and stopped by smgk.kernel.
"""

import os
import queue
import sys
from pathlib import Path
from typing import Dict, Optional

import nbformat
from nbformat import v4

from .kernel import State, connect, pid_alive
from .project import STATE_DIR

OUTPUT_TYPES = ("stream", "execute_result", "display_data", "error")


def _kind_of(msg: dict) -> str:
    md = msg["content"].get("metadata") or {}
    kind = (md.get("smgk") or {}).get("kind")
    if kind:
        return kind
    return "run" if msg["parent_header"].get("username") == "smgk" else "cell"


class NotebookBuilder:
    def __init__(self, nb: Optional[nbformat.NotebookNode] = None):
        self._nb = nb or v4.new_notebook(metadata={"language_info": {"name": "python"}})
        self._cells: Dict[str, nbformat.NotebookNode] = {
            c.metadata["smgk"]["msg_id"]: c for c in self._nb.cells if "smgk" in c.metadata
        }

    def notebook(self) -> nbformat.NotebookNode:
        return self._nb

    def _new_cell(self, parent: str, kind: str, cell: nbformat.NotebookNode) -> nbformat.NotebookNode:
        cell.metadata["smgk"] = {"msg_id": parent, "kind": kind}
        self._nb.cells.append(cell)
        self._cells[parent] = cell
        return cell

    def apply(self, msg: dict) -> bool:
        mtype = msg["msg_type"]
        parent = msg["parent_header"].get("msg_id")
        c = msg["content"]
        if not parent:
            return False

        if mtype == "execute_input":
            cell = self._cells.get(parent)
            if cell is None:
                cell = self._new_cell(parent, _kind_of(msg), v4.new_code_cell())
            cell.source = c["code"]
            cell.execution_count = c.get("execution_count")
            return True

        if mtype not in OUTPUT_TYPES:
            return False

        cell = self._cells.get(parent)
        if cell is None:
            kind = _kind_of(msg)
            if kind == "note":
                self._new_cell(parent, kind, v4.new_markdown_cell(c["data"].get("text/markdown", c["data"].get("text/plain", ""))))
                return True
            cell = self._new_cell(parent, kind, v4.new_code_cell())
        if cell.cell_type != "code":
            return False

        outs = cell.outputs
        if mtype == "stream":
            if outs and outs[-1].output_type == "stream" and outs[-1].name == c["name"]:
                outs[-1].text += c["text"]
            else:
                outs.append(v4.new_output("stream", name=c["name"], text=c["text"]))
        elif mtype == "error":
            outs.append(v4.new_output("error", ename=c["ename"], evalue=c["evalue"], traceback=c["traceback"]))
        elif mtype == "execute_result":
            outs.append(v4.new_output("execute_result", data=c["data"], metadata=c.get("metadata", {}),
                                      execution_count=c.get("execution_count")))
        else:
            outs.append(v4.new_output("display_data", data=c["data"], metadata=c.get("metadata", {})))
        return True


def _write(nb: nbformat.NotebookNode, path: Path) -> None:
    tmp = path.with_suffix(".ipynb.tmp")
    nbformat.write(nb, str(tmp))
    os.replace(tmp, path)


def run(root: Path) -> int:
    sd = root / STATE_DIR
    st = State.load(sd)
    if not st:
        return 1
    path = root / st.notebook
    builder = NotebookBuilder(nbformat.read(str(path), as_version=4) if path.exists() else None)
    kc = connect(root)

    # IOPub subscriptions are live only once a message arrives; our own kernel_info proves it
    probe = kc.kernel_info()
    ready = False
    dirty = False
    try:
        while True:
            try:
                msg = kc.get_iopub_msg(timeout=1)
            except queue.Empty:
                if not pid_alive(st.kernel_pid):
                    return 0
                continue
            if not ready and msg["parent_header"].get("msg_id") == probe:
                (sd / "recorder.ready").touch()
                ready = True
            dirty |= builder.apply(msg)
            if dirty and msg["msg_type"] == "status" and msg["content"]["execution_state"] == "idle":
                _write(builder.notebook(), path)
                dirty = False
    finally:
        if dirty:
            _write(builder.notebook(), path)
        kc.stop_channels()


if __name__ == "__main__":
    sys.exit(run(Path(sys.argv[1]).resolve()))
