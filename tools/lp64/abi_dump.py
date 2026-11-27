#!/usr/bin/env python3
"""Dump a struct's top-level field offsets exactly as the build compiles it.

Replays the TU's own compile command (from compile_commands.json) through
clang -fdump-record-layouts, so the layout reflects the real flags, forced
includes (gamespell.h, tu headers) and dsaddr copies -- not a clean header.

usage: abi_dump.py COMPDB TU_SUFFIX STRUCT [STRUCT...]   -> JSON on stdout
"""
import json, os, re, shlex, subprocess, sys

def compile_cmd(compdb, suffix):
    with open(compdb) as stream:
        entries = json.load(stream)
    for e in entries:
        if e["file"].endswith(suffix):
            args = e.get("arguments") or shlex.split(e.get("command", ""))
            if not args:  # Ninja compdb also lists phony dependency edges.
                continue
            return e["directory"], e["file"], args
    raise SystemExit("no compdb entry ends with %s" % suffix)

def layout_dump(directory, src, args):
    out, skip = ["clang"], False
    for a in args[1:]:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-c", "-MD", "-MMD") or a == src:
            continue
        out.append(a)
    out += ["-w", "-c", "-o", os.devnull, "-Xclang", "-fdump-record-layouts", src]
    r = subprocess.run(out, cwd=directory, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit("layout compiler failed (exit %d):\n%s" %
                         (r.returncode, r.stdout + r.stderr))
    return r.stdout + r.stderr

def fields(dump, struct):
    head = re.compile(r"^\s+0 \| (?:struct )?%s$" % re.escape(struct))
    lines, res, on = dump.splitlines(), [], False
    for ln in lines:
        if not on:
            on = bool(head.match(ln))
            continue
        m = re.match(r"^\s+(\d+) \|   (\S.*)$", ln)   # top-level field only
        if m:
            res.append((int(m.group(1)), m.group(2).strip()))
        if "sizeof=" in ln:
            size = int(re.search(r"sizeof=(\d+)", ln).group(1))
            align = re.search(r"\balign=(\d+)", ln)
            return {"size": size, "alignment": int(align.group(1)) if align else None,
                    "fields": res}
    return None

def main():
    compdb, suffix, structs = sys.argv[1], sys.argv[2], sys.argv[3:]
    d, src, args = compile_cmd(compdb, suffix)
    dump = layout_dump(d, src, args)
    result = {s: fields(dump, s) for s in structs}
    missing = [name for name, record in result.items() if record is None]
    if missing:
        raise SystemExit("compiler did not emit requested layouts: " + ", ".join(missing))
    print(json.dumps(result, indent=1))

if __name__ == "__main__":
    main()
