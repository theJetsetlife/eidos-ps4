"""Record the offline PSSL programs embedded by the PS4 platform library."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from check_sdk import sdk_version


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
