#!/usr/bin/env python3
"""Detect pointer arrays indexed with the DS's 4-byte stride (LP64 defect class).

Confirmed instance: btl_setup.c BTL_SETUP_SetTrainer --
    off = client * 4;  slots = (u8 *)param->trainerData;
    *(struct BattleTrainerData **)(slots + off) = ...;
A pointer-width store/load through a byte offset computed with *4 (or <<2)
straddles entries on LP64.

Patterns (candidates, not proof -- each needs a look):
  A  var = <e> * 4 | <e> << 2   ...later...   (T **)(<base> + var)
  B  (T **)(<base> + <e> * 4)   |   (T **)(<base> + (<e> << 2))
usage: detect_ptr_stride.py ROOT... > inventory.jsonl  (summary on stderr)
"""
import json, os, re, sys

PP = r"\(\s*(?:const\s+)?[\w ]+?\s*\*\s*\*\s*\)"          # (T **) cast
STRIDE_ASSIGN = re.compile(r"\b(\w+)\s*=\s*[^;=]*?(?:\*\s*4|<<\s*2)\s*;")
DIRECT = re.compile(PP + r"\s*\(\s*[^;]*?\+\s*[^;]*?(?:\*\s*4|<<\s*2)\b")
FUNC = re.compile(r"^[A-Za-z_][\w \*]*\b(\w+)\s*\([^;]*\)\s*\{?\s*$")

WIDE = re.compile(r"__SIZEOF_POINTER__\s*>\s*4|UINTPTR_MAX\s*>\s*UINT32_MAX")
NARROW = re.compile(r"__SIZEOF_POINTER__\s*(?:<=|==)\s*4|UINTPTR_MAX\s*<=\s*UINT32_MAX")

def scan(path):
    try:
        lines = open(path, errors="replace").read().splitlines()
    except OSError:
        return []
    out, func, stride_vars = [], None, {}
    # Preprocessor stack: each entry is "wide", "narrow" or None for the
    # active branch.  Code only compiled for 4-byte pointers is skipped (the
    # LP64 branch beside it is what the host build sees).
    stack = []
    for i, ln in enumerate(lines, 1):
        s = ln.strip()
        if s.startswith("#if"):
            stack.append("wide" if WIDE.search(s) else "narrow" if NARROW.search(s) else None)
            continue
        if s.startswith("#el") and stack:
            top = stack[-1]
            stack[-1] = "narrow" if top == "wide" else "wide" if top == "narrow" else None
            continue
        if s.startswith("#endif") and stack:
            stack.pop()
            continue
        if "narrow" in stack:
            continue
        if s.startswith(("*", "//", "/*")):
            continue
        m = FUNC.match(ln)
        if m:
            func, stride_vars = m.group(1), {}
        for v in STRIDE_ASSIGN.findall(ln):
            stride_vars[v] = i
        if DIRECT.search(ln):
            out.append(dict(file=path, line=i, func=func, pattern="B", snippet=s[:160]))
            continue
        for v, at in stride_vars.items():
            if re.search(PP + r"\s*\(\s*[^;]*?\+\s*" + re.escape(v) + r"\b", ln):
                out.append(dict(file=path, line=i, func=func, pattern="A",
                                var=v, var_line=at, snippet=s[:160]))
                break
    return out

def main():
    total, per = [], {}
    for root in sys.argv[1:]:
        for dp, _, fs in os.walk(root):
            for f in fs:
                if f.endswith(".c"):
                    for hit in scan(os.path.join(dp, f)):
                        total.append(hit)
                        per[hit["file"]] = per.get(hit["file"], 0) + 1
    for h in total:
        print(json.dumps(h))
    sys.stderr.write("candidates: %d in %d files\n" % (len(total), len(per)))
    for f, n in sorted(per.items(), key=lambda x: -x[1])[:25]:
        sys.stderr.write("  %3d %s\n" % (n, f))

if __name__ == "__main__":
    main()
