"""Record the offline PSSL programs embedded by the PS4 platform library."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess

from check_sdk import sdk_version


def source_dependencies(source):
    """Hash the local quoted includes used by the offline PSSL compiler."""
    seen = set()
    dependencies = []

    def visit(path):
        path = path.resolve()
        if path in seen:
            return
        seen.add(path)
        payload = path.read_bytes()
        for name in re.findall(r'^\s*#include\s+"([^"\n]+)"',
                               payload.decode("utf-8"), re.MULTILINE):
            child = (path.parent / name).resolve()
            if child not in seen:
                dependencies.append({
                    "source": Path(os.path.relpath(child, source.resolve().parent)).as_posix(),
                    "source_sha256": hashlib.sha256(child.read_bytes()).hexdigest(),
                })
                visit(child)

    visit(source)
    return dependencies


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--shader", nargs=3, action="append", required=True,
                        metavar=("SOURCE", "PROFILE", "BINARY"))
    args = parser.parse_args()
    version, _ = sdk_version(args.sdk)
    if version is None:
        parser.error("could not read Orbis SDK version")
    compiler = subprocess.check_output([str(args.compiler), "-version"], text=True).strip()
    shaders = []
    for source, profile, binary in sorted(args.shader):
        source = Path(source)
        binary = Path(binary)
        payload = binary.read_bytes()
        if not payload:
            parser.error(f"empty shader binary: {binary}")
        shaders.append({
            "id": source.stem,
            "profile": profile,
            "source": source.name,
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "source_dependencies": source_dependencies(source),
            "binary": binary.name,
            "binary_sha256": hashlib.sha256(payload).hexdigest(),
            "bytes": len(payload),
        })
    manifest = {"target": "Orbis", "sdk_version": version,
                "compiler": compiler, "shaders": shaders}
    temporary = args.out.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    temporary.replace(args.out)


if __name__ == "__main__":
    main()
