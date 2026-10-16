"""Workspace summary: what's in the kernel's namespace, for the viewer's Workspace panel.

`summarize` runs *inside the kernel* (its source is shipped over, since the kernel's env may
not have smgk installed), so it must stay self-contained: imports inside, no module globals.
"""

import ast
import inspect
from typing import Dict, List, Optional


def summarize(ns, hidden):
    """ns: name -> object; hidden: IPython's user_ns_hidden. Returns rows of name/type/size/value.
    No annotations: they'd name typing types the kernel's namespace doesn't have."""
    import inspect
    import sys
    import warnings

    max_names, max_value, max_minmax = 300, 60, 100_000
    np = sys.modules.get("numpy")
    array_modules = ("numpy", "torch", "jax", "jaxlib", "cupy")

    def clip(s):
        return s if len(s) <= max_value else s[:max_value] + "…"

    def fmt(x):
        return f"{x:g}" if isinstance(x, float) else str(x)

    def describe(v):
        t = type(v)
        root = t.__module__.split(".")[0]
        # repr()/len() only on types we know are cheap and side-effect free
        if t in (bool, int, float, complex):
            return "", clip(repr(v))
        if t in (str, bytes):
            return str(len(v)), clip(repr(v))
        if t in (list, tuple, dict, set, frozenset):
            return str(len(v)), ""
        if np is not None and isinstance(v, np.generic):
            return "", clip(repr(v.item()))
        if root == "pandas" and t.__name__ == "DataFrame":
            r, c = v.shape
            return f"{r}×{c}", ""
        if root == "pandas" and t.__name__ == "Series":
            return f"({len(v)},) {v.dtype}", ""
        if root in array_modules and hasattr(v, "shape") and hasattr(v, "dtype"):
            size = f"{tuple(int(d) for d in v.shape)} {v.dtype}".replace("torch.", "")
            value = ""
            if np is not None and isinstance(v, np.ndarray) and v.dtype.kind in "biuf" and 0 < v.size <= max_minmax:
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore")
                    lo, hi = np.nanmin(v), np.nanmax(v)
                if not (np.isnan(lo) or np.isnan(hi)):
                    value = f"min {fmt(lo.item())} … max {fmt(hi.item())}"
            return size, value
        return "", ""

    rows = []
    for name in sorted(ns):
        v = ns[name]
        if name.startswith("_") or (name in hidden and hidden[name] is v):
            continue
        if inspect.ismodule(v) or inspect.isroutine(v) or inspect.isclass(v):
            continue
        size, value = describe(v)
        rows.append({"name": name, "type": type(v).__name__, "size": size, "value": value})
        if len(rows) == max_names:
            break
    return rows


def probe_expression() -> str:
    """A user_expression that evaluates to the JSON summary, leaving no names in the namespace."""
    src = inspect.getsource(summarize) + "\nout = __import__('json').dumps(summarize(ip.user_ns, ip.user_ns_hidden))\n"
    return f"(lambda ns: exec({src!r}, ns) or ns['out'])({{'ip': get_ipython()}})"


def parse_reply(reply: dict) -> Optional[List[Dict[str, str]]]:
    """Rows from an execute_reply carrying probe_expression() as user_expressions['ws']."""
    import json

    expr = reply.get("content", {}).get("user_expressions", {}).get("ws", {})
    if expr.get("status") != "ok":
        return None
    try:
        return json.loads(ast.literal_eval(expr["data"]["text/plain"]))
    except (KeyError, ValueError, SyntaxError):
        return None
