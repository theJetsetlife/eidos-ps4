#!/usr/bin/env python3
"""Validate an official Orbis SDK installation without copying SDK data.

The report is intentionally limited to component presence, executable format,
host compatibility, and the public SDK version macro.  It never hashes or
dumps proprietary headers, libraries, samples, or credentials.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import re
import shutil
import sys


REQUIRED_TOOLS = {
    "C compiler": "orbis-clang.exe",
    "C++ compiler": "orbis-clang++.exe",
    "assembler": "orbis-as.exe",
    "archiver": "orbis-ar.exe",
    "linker": "orbis-ld.exe",
    "ranlib": "orbis-ranlib.exe",
    "object inspector": "orbis-readelf.exe",
    "ELF utility": "orbis-bin.exe",
    "PSSL compiler": "orbis-wave-psslc.exe",
}

REQUIRED_DIRS = {
    "target includes": "target/include",
    "common target includes": "target/include_common",
    "target libraries": "target/lib",
    "target modules": "target/sce_module",
}

REQUIRED_HEADERS = {
    "SDK version": "target/include/sdk_version.h",
    "kernel": "target/include/kernel.h",
    "video output": "target/include/video_out.h",
    "controller": "target/include/pad.h",
    "audio output": "target/include/audioout.h",
    "user service": "target/include/user_service.h",
    "save data": "target/include/save_data.h",
}

REQUIRED_LIBRARIES = {
    "kernel weak stub": "target/lib/libkernel_stub_weak.a",
    "video output weak stub": "target/lib/libSceVideoOut_stub_weak.a",
    "pad weak stub": "target/lib/libScePad_stub_weak.a",
    "audio output weak stub": "target/lib/libSceAudioOut_stub_weak.a",
    "user service weak stub": "target/lib/libSceUserService_stub_weak.a",
}

VERSION_RE = re.compile(r"SCE_ORBIS_SDK_VERSION\s+\(0x([0-9A-Fa-f]+)u?\)")


def candidates(explicit: str | None) -> list[Path]:
    values: list[Path] = []
    for raw in (explicit, os.environ.get("SCE_ORBIS_SDK_DIR")):
        if raw:
            values.append(Path(raw).expanduser())
    if os.name == "nt":
        for base in (os.environ.get("SCE_ROOT_DIR"), os.environ.get("ProgramFiles(x86)")):
            if base:
                values.append(Path(base) / "ORBIS SDKs" / "12.508")
    unique: list[Path] = []
    for value in values:
        resolved = value.resolve(strict=False)
        if resolved not in unique:
            unique.append(resolved)
    return unique


def executable_format(path: Path) -> str:
    try:
        with path.open("rb") as executable:
            magic = executable.read(4)
    except OSError:
        return "unreadable"
    if magic[:2] == b"MZ":
        return "windows-pe"
    if magic == b"\x7fELF":
        return "elf"
    return "unknown"


def sdk_version(root: Path) -> tuple[str | None, int | None]:
    header = root / "target/include/sdk_version.h"
    try:
        text = header.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None, None
    match = VERSION_RE.search(text)
    if not match:
        return None, None
    value = int(match.group(1), 16)
    return f"0x{value:08X}", value


def locate_packaging_tools() -> dict[str, str | None]:
    # Publishing Tools are a separate official installation.  Search PATH
    # only; do not guess a private installation layout or accept lookalikes.
    return {
        "package command": shutil.which("orbis_pub_cmd.exe") or shutil.which("orbis-pub-cmd.exe"),
        "package generator": shutil.which("orbispubgen.exe") or shutil.which("orbis-pub-gen.exe"),
    }


def inspect(root: Path) -> dict[str, object]:
    tools_dir = root / "host_tools/bin"
    tools: dict[str, object] = {}
    missing: list[str] = []
    formats: set[str] = set()
    for label, name in REQUIRED_TOOLS.items():
        path = tools_dir / name
        present = path.is_file()
        fmt = executable_format(path) if present else None
        if fmt:
            formats.add(fmt)
        tools[label] = {"present": present, "format": fmt}
        if not present:
            missing.append(label)

    components: dict[str, bool] = {}
    for label, rel in {**REQUIRED_DIRS, **REQUIRED_HEADERS, **REQUIRED_LIBRARIES}.items():
        path = root / rel
        present = path.is_dir() if label in REQUIRED_DIRS else path.is_file()
        components[label] = present
        if not present:
            missing.append(label)

    version_hex, version_value = sdk_version(root)
    expected_12508 = version_value is not None and (version_value >> 8) == 0x125080
    host = platform.system().lower()
    expected_format = {"linux": "elf", "windows": "windows-pe"}.get(host)
    invalid_formats = formats - {"elf", "windows-pe"}
    host_compatible = expected_format is not None and formats <= {expected_format}
    packaging = locate_packaging_tools()
    warnings: list[str] = []
    if invalid_formats:
        missing.append("readable SDK tools with recognized executable headers")
    if not host_compatible:
        warnings.append("SDK host tool formats are incompatible with the current host")
    for label, path in packaging.items():
        if path is None:
            warnings.append(f"official Publishing Tools component not found on PATH: {label}")
    if version_value is None:
        missing.append("parseable SDK version")
    elif not expected_12508:
        warnings.append("SDK is not from the requested 12.508 release line")

    return {
        "sdk_root": str(root),
        "sdk_version": version_hex,
        "requested_release_line": "12.508",
        "release_line_matches": expected_12508,
        "host": {"system": platform.system(), "machine": platform.machine()},
        "host_tools_compatible": host_compatible,
        "tools": tools,
        "components": components,
        "packaging_tools": packaging,
        "missing": sorted(set(missing)),
        "warnings": warnings,
        "usable_for_target_build": not missing and host_compatible and expected_12508,
    }


def print_text(report: dict[str, object]) -> None:
    print(f"SDK root: {report['sdk_root']}")
    print(f"SDK version: {report['sdk_version'] or 'unknown'}")
    host = report["host"]
    assert isinstance(host, dict)
    print(f"Host: {host['system']} {host['machine']}")
    for label, value in report["tools"].items():
        assert isinstance(value, dict)
        state = "ok" if value["present"] else "missing"
        suffix = f" ({value['format']})" if value["format"] else ""
        print(f"tool: {label}: {state}{suffix}")
    for label, present in report["components"].items():
        print(f"component: {label}: {'ok' if present else 'missing'}")
    for label, path in report["packaging_tools"].items():
        print(f"component: {label}: {'ok' if path else 'missing'}")
    for warning in report["warnings"]:
        print(f"warning: {warning}")
    if report["missing"]:
        print("missing: " + ", ".join(report["missing"]))
    print("target build: " + ("ready" if report["usable_for_target_build"] else "not ready"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk", help="SDK root; otherwise use SCE_ORBIS_SDK_DIR")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    parser.add_argument(
        "--allow-incompatible-host",
        action="store_true",
        help="do not fail solely because installed host tools cannot run here",
    )
    args = parser.parse_args()
    roots = candidates(args.sdk)
    root = next((item for item in roots if item.is_dir()), None)
    if root is None:
        print("error: official SDK root not found; pass --sdk or set SCE_ORBIS_SDK_DIR", file=sys.stderr)
        return 2
    report = inspect(root)
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print_text(report)
    if report["missing"] or not report["release_line_matches"]:
        return 1
    if not report["host_tools_compatible"] and not args.allow_incompatible_host:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
