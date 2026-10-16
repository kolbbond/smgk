# smgk

Notebook output without leaving nvim + tmux. Each project gets one Jupyter kernel; an
[smg](https://github.com/kolbbond/simple-magnum-gui) window watches it and shows every
cell's code, text, errors and plots, and a recorder saves the session as `.ipynb`.

```
nvim --(slime)--> jupyter console --> ipykernel --(IOPub)--> smgk-view
agent / C++ ------(smgk push|note|run)---^   \--> recorder --> notebooks/*.ipynb
```

## Install

```bash
# smg, installed to ~/.local/opt/smg (from the smg repo)
make && cmake --install build --prefix ~/.local/opt/smg

# smgk: smgk-view to ~/.local/bin, the smgk CLI via pip --user
make install
python3 -m pip install --user jupyter-console
```

The kernel runs in `$VIRTUAL_ENV` when set (it needs `ipykernel` there), else the user python.

## Commands

All of them work from anywhere inside the project (git root, else the directory `start` ran in).

| command | |
|---|---|
| `smgk start` / `stop` / `restart` / `status` | kernel + recorder lifecycle; `start` prints the connection file |
| `smgk view` | open the viewer (one per project) |
| `smgk push out.png` | show an image file in the feed (C++ codes, scripts) |
| `smgk note "**text**"` | add a markdown note (the agent annotating its work) |
| `smgk run file.py` / `smgk run -c "code"` | run code on the kernel; output prints here and lands in the feed |

State lives in `<project>/.smgk/` (logs too); notebooks in `<project>/notebooks/`, one per
kernel start.

## nvim

In `after/plugin/slime.lua` (nvim-config):

| key | |
|---|---|
| `<leader>vk` / `<leader>hk` | start the project kernel, open `jupyter console` in a tmux split, open the viewer |
| `<leader>sc`, `<C-l>`, `<leader>sf`, ... | send cells/lines as usual |
| `<leader>sr` / `<leader>so` | restart the kernel (new notebook) / and rerun the file |
| `<leader>vp` / `<leader>hp` | plain ipython, unchanged |

## Viewer

Click a plot to open it full size; mouse wheel zooms, Esc closes. The feed follows new output
unless you've scrolled up. The dot in the top bar is the kernel state (green idle, amber busy).

## Workspace panel

MATLAB-style variable list on the right of the viewer (`Ctrl+W` toggles it): name, type,
size (`(1000, 3) float64`, `500×12` for DataFrames, lengths for containers) and a short value
(scalars, short strings, min/max for small numeric arrays). Click a header to sort, type to
filter, hover a row to see it whole. It refreshes after each cell; `push`/`note` don't touch it.

The recorder asks the kernel for this summary with a silent request after each cell and writes
`.smgk/workspace.json`. It never calls `repr()` on objects it doesn't know (only builtins,
numpy, pandas, and array-likes with `shape`/`dtype`), hides `_names`, modules, functions,
classes and IPython's own names, and lists at most 300 variables.

## Tests

```bash
make test                 # C++ unit + gui (needs a display; WSLg works)
cd python && python3 -m pytest -q
```
