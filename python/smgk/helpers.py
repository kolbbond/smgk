"""push / note / run: put things in the feed by executing on the project kernel."""

import base64
from pathlib import Path

import sys
from typing import Optional

from .kernel import SmgkError, connect

# push/note wait this long; the request stays queued behind a busy cell either way
QUEUE_TIMEOUT = 30


def _quiet(_msg):
    pass


def _execute(start: Path, code: str, silent: bool, timeout: Optional[float]) -> Optional[dict]:
    kc = connect(start, username="smgk")
    try:
        kw = {"output_hook": _quiet} if silent else {}
        return kc.execute_interactive(code, silent=silent, store_history=not silent, timeout=timeout, **kw)
    except TimeoutError:
        print("smgk: kernel busy; queued, it appears when the current cell finishes", file=sys.stderr)
        return None
    finally:
        kc.stop_channels()


# helper code runs in the user namespace, so it lives in a function it deletes
_PUSH = """def _smgk_push():
    import base64
    from IPython.display import Image, display
    display(Image(data=base64.b64decode({b64!r})), metadata={{"smgk": {{"kind": "push", "file": {name!r}}}}})
_smgk_push(); del _smgk_push"""

_NOTE = """def _smgk_note():
    from IPython.display import display
    display({{"text/markdown": {text!r}, "text/plain": {text!r}}}, raw=True, metadata={{"smgk": {{"kind": "note"}}}})
_smgk_note(); del _smgk_note"""


def push(start: Path, image: Path) -> None:
    image = Path(image).resolve()
    if not image.is_file():
        raise SmgkError(f"smgk: no such file: {image}")
    b64 = base64.b64encode(image.read_bytes()).decode()
    _execute(start, _PUSH.format(b64=b64, name=image.name), silent=True, timeout=QUEUE_TIMEOUT)


def note(start: Path, text: str) -> None:
    _execute(start, _NOTE.format(text=text), silent=True, timeout=QUEUE_TIMEOUT)


def run(start: Path, code: str) -> bool:
    """Returns False when the code raised (the traceback is in the feed)."""
    reply = _execute(start, code, silent=False, timeout=None)  # analyses can run for hours
    return reply is not None and reply["content"]["status"] == "ok"
