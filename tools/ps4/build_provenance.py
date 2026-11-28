#!/usr/bin/env python3
"""Generate the PS4 runtime build provenance string."""

import argparse
import hashlib
import json
from pathlib import Path
import re

from check_sdk import sdk_version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--eidos", required=True)
    parser.add_argument("--eidos-tree", required=True)
    parser.add_argument("--aletheia", required=True)
    parser.add_argument("--aletheia-tree", required=True)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--build-type", default="")
    parser.add_argument("--release", choices=("0", "1"), required=True)
    args = parser.parse_args()

    manifest_bytes = args.manifest.read_bytes()
    manifest = json.loads(manifest_bytes)
    sdk_macro, _ = sdk_version(args.sdk_root)
    sdk_header = (args.sdk_root / "target/include/sdk_version.h").read_text(
        encoding="utf-8-sig", errors="replace"
    )
    sdk_release = re.search(r"Programmer Tool Runtime Library Release\s+([0-9.]+)", sdk_header)
    provenance = {
        "eidos_revision": args.eidos,
        "eidos_tree": args.eidos_tree,
        "aletheia_revision": args.aletheia,
        "aletheia_tree": args.aletheia_tree,
        "build_type": args.build_type or "unspecified",
        "release": args.release == "1",
        "sdk": "Orbis-" + (sdk_release.group(1) if sdk_release else "unknown")
        + " (" + (sdk_macro or "version-macro-unknown") + ")",
        "renderer": "GNMX-DS-composite;G3D=CPU-raster",
        "shader_manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
        "shader_count": len(manifest.get("shaders", [])),
        "game_data": "user-selected-at-runtime",
        "runtime_options": "console-launch-arguments",
    }
    c_string = json.dumps(json.dumps(provenance, sort_keys=True), ensure_ascii=True)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(
        "/* Generated from the build and embedded PSSL manifest. */\n"
        '#include "log_ps4.h"\n'
        f"static const char eidos_provenance[] = {c_string};\n"
        "const char *eidos_ps4_build_provenance(void)\n"
        "{\n    return eidos_provenance;\n}\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
