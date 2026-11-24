#!/usr/bin/env python3
"""Compile one PSSL source with the official Orbis shader compiler.

The compiler is a host tool distributed with the official SDK. This wrapper
does not copy SDK files and does not accept a fallback compiler.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--profile", required=True,
                        choices=("sce_vs_vs_lvp", "sce_ps_lvp"))
    parser.add_argument("--cache-dir", required=True, type=Path)
    parser.add_argument("--include-dir", action="append", default=[], type=Path)
    parser.add_argument("--auxdata", action="store_true",
                        help="append the SDK's optional ShaderAuxData table")
    args = parser.parse_args()

    for label, path in (("compiler", args.compiler), ("source", args.source)):
        if not path.is_file():
            parser.error(f"{label} does not exist: {path}")
    for path in args.include_dir:
        if not path.is_dir():
            parser.error(f"include directory does not exist: {path}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.cache_dir.mkdir(parents=True, exist_ok=True)
    command = [str(args.compiler)]
    for path in args.include_dir:
        command.append(f"-I{path}")
    command.extend(("-profile", args.profile, "-o", str(args.output)))
    if args.auxdata:
        command.append("--auxdata")
    command.extend((str(args.source), "-cache", "-cachedir", str(args.cache_dir)))

    try:
        result = subprocess.run(command, check=False)
    except OSError as exc:
        print(f"PSSL compiler could not be started: {exc}", file=sys.stderr)
        return 2
    if result.returncode != 0:
        return result.returncode if result.returncode > 0 else 2
    if not args.output.is_file() or args.output.stat().st_size == 0:
        print("PSSL compiler succeeded without producing a non-empty output",
              file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
