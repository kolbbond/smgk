"""smgk command line."""

import argparse
import sys
from pathlib import Path

from . import kernel
from .project import find_root


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="smgk")
    sub = p.add_subparsers(dest="cmd", required=True)
    for name in ("start", "stop", "restart", "status"):
        sp = sub.add_parser(name)
        sp.add_argument("--dir", type=Path, default=Path.cwd())
    args = p.parse_args(argv)

    try:
        root = find_root(args.dir)
        if args.cmd in ("start", "restart"):
            st = getattr(kernel, args.cmd)(root)
            print(root / st.connection_file)
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
