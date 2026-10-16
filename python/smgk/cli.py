"""smgk command line."""

import argparse
import sys
from pathlib import Path

from . import helpers, kernel, viewer
from .project import find_root


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="smgk")
    sub = p.add_subparsers(dest="cmd", required=True)
    for name in ("start", "stop", "restart", "status"):
        sp = sub.add_parser(name)
        sp.add_argument("--dir", type=Path, default=Path.cwd())
    sp = sub.add_parser("view", help="open the smgk-view window for this project")
    sp.add_argument("--dir", type=Path, default=Path.cwd())
    sp.add_argument("extra", nargs=argparse.REMAINDER, help="passed to smgk-view (after --)")
    sp = sub.add_parser("push", help="show an image file in the feed")
    sp.add_argument("file", type=Path)
    sp = sub.add_parser("note", help="add a markdown note to the feed")
    sp.add_argument("text")
    sp = sub.add_parser("run", help="run a python file or -c code on the kernel")
    g = sp.add_mutually_exclusive_group(required=True)
    g.add_argument("file", type=Path, nargs="?")
    g.add_argument("-c", dest="code")
    args = p.parse_args(argv)

    try:
        if args.cmd == "push":
            helpers.push(Path.cwd(), args.file)
            return 0
        if args.cmd == "note":
            helpers.note(Path.cwd(), args.text)
            return 0
        if args.cmd == "run":
            if args.file and not args.file.is_file():
                raise kernel.SmgkError(f"smgk: no such file: {args.file.resolve()}")
            return 0 if helpers.run(Path.cwd(), args.code if args.code is not None else args.file.read_text()) else 1

        root = find_root(args.dir)
        if args.cmd in ("start", "restart"):
            st = getattr(kernel, args.cmd)(root)
            print(root / st.connection_file)
        elif args.cmd == "view":
            viewer.launch(root, [a for a in args.extra if a != "--"])
        elif args.cmd == "stop":
            kernel.stop(root)
        elif args.cmd == "status":
            st = kernel.status(root)
            if not st:
                print(f"no kernel for {root}")
                return 1
            print(f"generation {st.generation}  kernel pid {st.kernel_pid}  notebook {root / st.notebook}")
    except kernel.SmgkError as e:
        print(e.msg, file=sys.stderr)
        return e.code
    return 0


if __name__ == "__main__":
    sys.exit(main())
