#!/usr/bin/env python3
"""Embed an official PSSL compiler output as a generated C++ byte array.

This converts a build artifact only; it does not parse, alter, or synthesize
the proprietary Sony shader-binary format.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")
BYTES_PER_LINE = 12


def render(source: Path, symbol: str) -> str:
    if not IDENTIFIER.fullmatch(symbol):
        raise ValueError(f"invalid C++ symbol name: {symbol!r}")
    payload = source.read_bytes()
    if not payload:
        raise ValueError(f"shader binary is empty: {source}")

    rows = []
    for offset in range(0, len(payload), BYTES_PER_LINE):
        chunk = payload[offset:offset + BYTES_PER_LINE]
        rows.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk))
    data = ",\n".join(rows)
    guard = f"EIDOS_PS4_EMBEDDED_{symbol.upper()}_H"
    return (
        f"#ifndef {guard}\n"
        f"#define {guard}\n\n"
        "#include <stddef.h>\n"
        "#include <stdint.h>\n\n"
        "#if defined(__cplusplus)\n"
        "#define EIDOS_PS4_SHADER_ALIGN alignas(16)\n"
        "#else\n"
        "#define EIDOS_PS4_SHADER_ALIGN _Alignas(16)\n"
        "#endif\n\n"
        f"EIDOS_PS4_SHADER_ALIGN static const uint8_t {symbol}[] = {{\n"
        f"{data}\n"
        "};\n"
        f"static const size_t {symbol}_size = sizeof({symbol});\n\n"
        "#undef EIDOS_PS4_SHADER_ALIGN\n"
        f"#endif /* {guard} */\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--symbol", required=True)
    args = parser.parse_args()
    if not args.input.is_file():
        parser.error(f"shader binary does not exist: {args.input}")
    try:
        content = render(args.input, args.symbol)
    except (OSError, ValueError) as exc:
        print(f"shader embedding failed: {exc}", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_text(content, encoding="ascii", newline="\n")
    temporary.replace(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
