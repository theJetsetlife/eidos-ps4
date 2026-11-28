#!/usr/bin/env python3
"""Generate the PS4 runtime build provenance string."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from check_sdk import sdk_version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--eidos-root", required=True, type=Path)
    parser.add_argument("--aletheia-root", required=True, type=Path)
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

    def git_identity(root):
        try:
            revision = subprocess.check_output(
                ["git", "-C", str(root), "rev-parse", "HEAD"],
                text=True, stderr=subprocess.DEVNULL,
            ).strip()
            dirty = subprocess.check_output(
                ["git", "-C", str(root), "status", "--porcelain"],
                text=True, stderr=subprocess.DEVNULL,
            ).strip()
        except (OSError, subprocess.CalledProcessError):
            return "unknown", "unknown"
        return revision, "dirty" if dirty else "clean"

    eidos_revision, eidos_tree = git_identity(args.eidos_root)
    aletheia_revision, aletheia_tree = git_identity(args.aletheia_root)
    provenance = {
        "eidos_revision": eidos_revision,
        "eidos_tree": eidos_tree,
        "aletheia_revision": aletheia_revision,
        "aletheia_tree": aletheia_tree,
        "build_type": args.build_type or "unspecified",
        "release": args.release == "1",
        "sdk": "Orbis-" + (sdk_release.group(1) if sdk_release else "unknown")
        + " (" + (sdk_macro or "version-macro-unknown") + ")",
        "renderer": "GNMX-DS-composite;G3D=CPU-raster;optional-experimental-native-3D-2D",
        "shader_manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
        "shader_count": len(manifest.get("shaders", [])),
        "game_data": "user-selected-at-runtime",
        "runtime_options": "--g3d-snapshot-flat;--ps4-native-3d;/data/eidos/native-3d.enable",
    }
    c_string = json.dumps(json.dumps(provenance, sort_keys=True), ensure_ascii=True)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    generated = (
        "/* Generated from the build and embedded PSSL manifest. */\n"
        '#include "log_ps4.h"\n'
        f"static const char eidos_provenance[] = {c_string};\n"
        "const char *eidos_ps4_build_provenance(void)\n"
        "{\n    return eidos_provenance;\n}\n"
    )
    if not args.out.exists() or args.out.read_text(encoding="utf-8") != generated:
        args.out.write_text(generated, encoding="utf-8")


if __name__ == "__main__":
    main()
