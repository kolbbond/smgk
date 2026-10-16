import os
import signal
from pathlib import Path

import pytest

from smgk import viewer

BIN = Path(__file__).parents[2] / "build" / "smgk-view"


@pytest.mark.skipif(not BIN.exists(), reason="smgk-view not built")
def test_view_launch_idempotent(project, monkeypatch):
    monkeypatch.setenv("SMGK_VIEW", str(BIN))
    (project / ".smgk").mkdir()
    a = viewer.launch(project, ["--smg-hidden"])
    try:
        b = viewer.launch(project, ["--smg-hidden"])
        assert a == b
        assert int((project / ".smgk" / "view.pid").read_text().split()[0]) == a
    finally:
        os.kill(a, signal.SIGTERM)


def test_view_missing_binary(project, monkeypatch):
    monkeypatch.setenv("SMGK_VIEW", "/nonexistent/smgk-view")
    (project / ".smgk").mkdir()
    with pytest.raises(SystemExit) as exc:
        viewer.launch(project, [])
    assert "smgk-view not found" in exc.value.msg
