import json
import math
import os
import time
import types

import numpy as np
import pandas as pd
import pytest

from smgk import helpers, kernel
from smgk.workspace import summarize


def by_name(rows):
    return {r["name"]: r for r in rows}


def test_summarize_scalars_and_strings():
    rows = by_name(summarize({"n": 5, "x": 2.5, "flag": True, "s": "hello"}, {}))
    assert rows["n"] == {"name": "n", "type": "int", "size": "", "value": "5"}
    assert rows["x"]["value"] == "2.5"
    assert rows["flag"]["value"] == "True"
    assert rows["s"]["size"] == "5" and rows["s"]["value"] == "'hello'"


def test_summarize_long_string_truncated():
    rows = by_name(summarize({"s": "a" * 500}, {}))
    assert len(rows["s"]["value"]) <= 61 and rows["s"]["value"].endswith("…")


def test_summarize_numpy():
    rows = by_name(summarize({"a": np.arange(6.0).reshape(2, 3), "big": np.zeros(2_000_000), "f": np.float64(1.5),
                              "i": np.int64(7)}, {}))
    assert rows["a"]["type"] == "ndarray"
    assert rows["a"]["size"] == "(2, 3) float64"
    assert rows["a"]["value"] == "min 0 … max 5"
    assert rows["big"]["size"] == "(2000000,) float64" and rows["big"]["value"] == ""
    assert rows["f"]["value"] == "1.5"
    assert rows["i"]["value"] == "7"


def test_summarize_pandas():
    df = pd.DataFrame({"a": range(500), "b": 0.0})
    rows = by_name(summarize({"df": df, "col": df["a"]}, {}))
    assert rows["df"]["type"] == "DataFrame" and rows["df"]["size"] == "500×2"
    assert rows["col"]["type"] == "Series" and rows["col"]["size"] == "(500,) int64"


def test_summarize_containers():
    rows = by_name(summarize({"l": [1, 2, 3], "d": {"a": 1}, "t": (1,), "st": {1, 2}}, {}))
    assert [rows[k]["size"] for k in ("l", "d", "t", "st")] == ["3", "1", "1", "2"]


def test_summarize_hides_noise():
    hidden = {"In": [], "Out": {}}
    ns = {"_private": 1, "np": np, "f": lambda: 1, "C": type("C", (), {}), "In": hidden["In"], "Out": hidden["Out"], "keep": 1}
    assert [r["name"] for r in summarize(ns, hidden)] == ["keep"]


def test_summarize_never_calls_repr_on_unknown_objects():
    class Explosive:
        def __repr__(self):
            raise AssertionError("repr called")

        def __len__(self):
            raise AssertionError("len called")

    rows = by_name(summarize({"boom": Explosive()}, {}))
    assert rows["boom"] == {"name": "boom", "type": "Explosive", "size": "", "value": ""}


def test_summarize_caps_names():
    rows = summarize({f"v{i:04d}": i for i in range(1000)}, {})
    assert len(rows) == 300 and rows[0]["name"] == "v0000"


def test_summarize_nan_and_inf():
    rows = by_name(summarize({"a": np.array([1.0, math.nan, 3.0]), "z": np.array([], dtype=float)}, {}))
    assert rows["a"]["value"] == "min 1 … max 3"
    assert rows["z"]["value"] == ""


def _wait_workspace(project, pred, timeout=10):
    path = project / ".smgk" / "workspace.json"
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            ws = json.loads(path.read_text())
            if pred(ws):
                return ws
        except (OSError, ValueError):
            pass
        time.sleep(0.1)
    raise AssertionError(f"workspace never matched; last: {path.read_text() if path.exists() else None}")


def test_workspace_follows_cells(project):
    st = kernel.start(project)
    ws = _wait_workspace(project, lambda w: w["generation"] == st.generation)
    assert ws["vars"] == []
    helpers.run(project, "import numpy as np\na = np.zeros((10, 3))\nn = 5\n_hidden = 1")
    ws = _wait_workspace(project, lambda w: len(w["vars"]) == 2)
    rows = by_name(ws["vars"])
    assert rows["a"]["size"] == "(10, 3) float64" and rows["n"]["value"] == "5"


def test_workspace_ignores_push_and_note(project):
    kernel.start(project)
    helpers.run(project, "n = 1")
    _wait_workspace(project, lambda w: len(w["vars"]) == 1)
    path = project / ".smgk" / "workspace.json"
    time.sleep(1.0)
    before = os.stat(path).st_mtime_ns
    helpers.note(project, "hi")
    time.sleep(1.5)
    assert os.stat(path).st_mtime_ns == before


def test_probe_runs_in_bare_namespace():
    # the kernel evaluates the probe with only the user's names around: no typing, no smgk
    class FakeIP:
        user_ns = {"n": 1, "In": []}
        user_ns_hidden = {"In": user_ns["In"]}

    user_ns = {"get_ipython": lambda: FakeIP}
    out = eval(__import__("smgk.workspace", fromlist=["x"]).probe_expression(), user_ns)
    assert json.loads(out) == [{"name": "n", "type": "int", "size": "", "value": "1"}]
    assert set(user_ns) == {"get_ipython", "__builtins__"}
