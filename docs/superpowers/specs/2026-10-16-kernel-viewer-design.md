# Design: smgk, a kernel viewer on smg (notebook output window)

Date: 2026-10-16
Status: draft, pending approval (builder check done)

## Intent

Get notebook-style output without leaving nvim + tmux. The user writes ordinary `.py`
files with `# %%` cells and runs them from nvim (vim-slime, already set up). Today plots
pop up and vanish, text scrolls away in the tmux pane, and C++ codes and the AI agent
drop images into folders. The wanted experience: one persistent smg window beside tmux
that shows everything that ran, in order (code, text output, errors, plots rendered as
images), the way a Jupyter notebook does, while editing stays in nvim.

The user's long-term hope is that the window becomes the notebook itself (holds and runs
cells). Phase 1 is a watcher only; phase 2 is out of scope here but must not require a
rewrite.

## Repo

`smgk` lives in its own repo (`~/programs/cpp/smgk`), **not** inside smg: the user
wants smg free of the Jupyter/ZeroMQ dependencies. smgk consumes smg as an ordinary
library (`find_package(smg)` against an installed prefix, the pattern in smg's
`test/consumer/`). The Python pieces (recorder, helper CLI) live in this repo too. The
name `smgk` is Claude's call.

## Core idea

A Jupyter kernel is the hub. Every kernel broadcasts all execution inputs and outputs on
its IOPub channel (ZeroMQ PUB socket) to any number of subscribers. So:

```
nvim --(slime)--> jupyter console  --(shell)-->  ipykernel  --(IOPub broadcast)--> smg viewer
agent / C++ ----(helper CLI)--------(shell)-->      |                         \--> (later) browser
```

- One `ipykernel` per project: the git root, else the directory it was started in.
  Connection file and session state live in `<project>/.smgk/` (name is builder's call).
  Every client (viewer, helper CLI) finds the kernel by walking up from its cwd to the
  nearest `.smgk/`, the same rule nvim's `<leader>rm` uses for Makefiles. Nothing is
  routed by "most recent kernel", so pushes can't silently land in the wrong project.
- **A small Python recorder** runs beside the kernel (started with it, by the helper
  CLI). It subscribes to IOPub and is the only writer of the session `.ipynb`, so the
  record is complete even when no viewer is open.
- **Input path is unchanged in spirit:** slime sends cells to a `jupyter console
  --existing` running in the tmux REPL pane (instead of a standalone `ipython`). Text
  output still shows in that pane.
- **The smg viewer subscribes to IOPub** and renders the feed. On open it first loads
  the recorder's `.ipynb` (history so far), then follows IOPub live. It sees code run by any
  client: the console, the agent, a second console.
- Matplotlib needs no hook: ipykernel's default inline backend emits figures as
  `display_data` with an `image/png` payload.

## Phase 1 goals

1. **Viewer app (C++, links smg)** that, given a kernel connection file, subscribes to IOPub and
   shows a scrolling feed of entries:
   - `execute_input` → the cell's code (with execution count)
   - `stream` (stdout/stderr) → text, appended to the current cell's entry
   - `execute_result` / `display_data` → `image/png` as an image (decoded to a texture),
     otherwise `text/plain`
   - `error` → traceback (strip ANSI color codes or render them; builder's call)
   - outputs are grouped under the cell that produced them (match on the message's
     `parent_header.msg_id`)
   - kernel status (busy/idle) visible somewhere small
2. **Images:** fit to the feed width, click to view larger (zoom/pan in a separate panel
   or popup). Plots are the main reason this exists, so this must feel good.
3. **The session is saved as a real `.ipynb`** (nbformat 4) by the recorder, so it can
   be reopened in Jupyter/VSCode or sent to someone. One file per kernel start, under
   `<project>/notebooks/<date>-<time>.ipynb` (builder may refine naming). A kernel
   restart starts a new file. *Claude's call; the user did not answer this question but
   did not object. Recorder-not-viewer is Claude's call after review: IOPub has no
   replay, so a viewer-written file would miss anything run while the viewer was
   closed.*
4. **Helper CLI** (Python, uses `jupyter_client`) so non-console clients can put things
   in the feed by executing on the kernel, which makes them appear for every viewer:
   - `push <image file>` → runs `display(Image(filename=...))` (C++ codes' PNG output)
   - `note "<text>"` → displays markdown/text (the agent annotating what it did)
   - `run <file.py | code>` → executes code (the agent running analysis)
   The agent and C++ workflows use this; no separate server.
   Helper-originated entries are tagged in message metadata (e.g. `smgk: note|push`) so
   the viewer and recorder show a `note` as a markdown cell and a `push` as an
   output-only entry, never as the wrapper code. Notes emit `text/markdown` with a
   readable `text/plain` fallback. *Claude's call.*
5. **nvim integration** (in `~/defcon/nvim-config`, `after/plugin/slime.lua`): a **new,
   separate** keymap that starts the project kernel + recorder (if not running), opens
   `jupyter console --existing <file>` in the slime pane, and launches the viewer. The
   existing ipython flow (`<leader>vp`/`<leader>hp`) stays as-is; the user hasn't
   decided whether the kernel flow should become the default, so it must be easy to
   promote later. Send keys (`<leader>sc`, `<C-l>`, ...) work in both. slime must
   remember which kind of REPL the current target pane is and set
   `g:slime_python_ipython` accordingly (1 for ipython's `%cpaste`, 0 for the console),
   and `<leader>sr`/`<leader>so` restart whichever kind is current. For the kernel flow,
   restart means restarting the kernel (fresh state, which is the point of
   restart-and-send); the viewer must handle that (execution count resets, new notebook
   file).
6. **Python environment:** the kernel runs in `$VIRTUAL_ENV`'s python when set (matching
   slime's current behavior), else the user python. If `ipykernel` is missing in that
   env, fail with a message naming the exact install command. *Claude's call.*

## Non-goals (phase 1)

- Sending code from the viewer, editing cells in the viewer (that is phase 2).
- Browser viewer. Deferred: browsers can't speak ZeroMQ, it needs a small bridge.
  *Claude's call, flagged to the user.*
- Rich MIME beyond PNG and plain text (HTML tables, SVG, LaTeX, widgets). Fall back to
  `text/plain`.
- Multi-kernel tabs. One viewer watches one kernel.
- Remote kernels (goose boxes over SSH). The user works on WSL2 locally; not needed yet.
  Nothing should preclude it later.

## Anti-goals

- Must not change how the user writes code: plain `.py` + `# %%`, no viewer-specific
  calls needed to get plots shown.
- Must not make slime/ipython slower or flakier; if the viewer is closed or crashes,
  nvim + console keep working untouched, and the notebook record keeps being written.
- Must not disturb the existing ipython flow.
- Must not add dependencies to smg. If smgk needs something smg doesn't expose (e.g.
  decoding a PNG from memory into a texture), prefer doing it in smgk; a small,
  dependency-free addition to smg is acceptable only if it's generally useful.

## Constraints and known facts

- Platform: WSL2 with WSLg (`DISPLAY=:0`, Wayland available). The viewer is a native
  Linux window.
- Python side present: ipykernel 7.1, jupyter_client 8.8, pyzmq 27.1. **Not present:**
  `jupyter-console` (pip install), libzmq C headers (no `zmq.h`, no `libzmq.pc`).
- smg: C++ / Magnum + ImGui + ImPlot. PNG decoding is available via Magnum
  `AnyImageImporter` + `StbImageImporter` behind `SMG_WITH_IMAGE_IMPORT` (currently used by
  `SpriteSheet::load(path)`, file-based; decoding from memory is needed here).
  No JSON library is vendored yet. Externals are vendored under `external/`.
- smg C++ style (`.clang-format` / `.clang-tidy` in smg's root) applies to smgk too;
  copy them in.
- Jupyter wire protocol: multipart messages with HMAC-SHA256 signatures. A pure IOPub
  subscriber may skip signature verification (local trust), but phase 2 sending will need
  signing; choose the crypto/zmq dependencies with that in mind.
- `jupyter console` runs magics in the kernel, so IPython's terminal `%cpaste` path is
  unavailable. slime must use bracketed paste for the console
  (`g:slime_bracketed_paste = 1`, already set) with `g:slime_python_ipython = 0` when
  targeting the console. Verify multi-line indented cells execute.

## Decided

- Host: an smg-based native window, in a separate `smgk` repo. The user specifically liked "smg as an ipykernel watcher".
- Architecture: kernel as hub, viewer subscribes to IOPub directly from C++.
  *Claude's call on "directly from C++" vs a Python sidecar feeding smg; the user's
  enthusiasm was for the direct watcher. A sidecar is the fallback if native ZeroMQ
  proves painful.*
- Input stays in nvim via slime → `jupyter console`.
- Phasing: watcher first, notebook later.
- Local only (WSL2); remote deferred.
- Kernel flow sits beside the ipython REPL on its own key for now.

## Left to the builder

- smgk's internal layout, target names.
- How smg, libzmq and a JSON parser are brought in (installed smg via find_package,
  FetchContent, submodule, or system package), as long as it builds on the user's machine.
- Feed layout and styling, ANSI handling, image zoom UI.
- Exact helper CLI name and where it's installed (likely `~/.local/bin`).
- How the kernel is started (helper CLI subcommand is a natural fit) and where
  connection files live.
- Testing approach (smg has headless capture for CI; a fake IOPub publisher in tests is
  a reasonable route).
