#!/usr/bin/env python3
"""Detect calls through DS-bound function-pointer tables (LP64 defect class).

A symbol bound BW_DATA_BIND_DS is the raw DS bytes: its "function pointers"
are 4-byte DS code addresses and its entries have the DS stride.  Calling
`TABLE[i](...)` / `TABLE[i][j](...)` / `TABLE[i].fn(...)` on LP64 reads an
8-byte slot at the native stride and jumps to junk.  Confirmed instances:
ov021 data_021D3CE8 (GetFrontTileMessage), ov010 d_021725C0
(EVENT_BATTLE_DispatchBattleKindNotify).  Fix pattern: read the DS words and
resolve each with bw_ovl_code_entry / bw_arm9_code_entry (reimpl_vm_control.c
bw_vm_command), via a port patch or a dsaddr rule.

usage: detect_ds_fntable.py BUILD_DIR > hits.jsonl   (summary on stderr)
"""
import glob, json, os, re, sys

BIND = re.compile(r'\{\s*"(\w+)",\s*0x[0-9A-Fa-f]+u?,\s*&bw_dsref_\w+,\s*BW_DATA_BIND_DS\b')
CALL = re.compile(r"\b([A-Za-z_]\w*)\s*((?:\[[^\[\]]*\]\s*)+)(?:(?:\.|->)\s*(\w+)\s*)?\(")
WIDE = re.compile(r"__SIZEOF_POINTER__\s*>\s*4|UINTPTR_MAX\s*>\s*UINT32_MAX")
NARROW = re.compile(r"__SIZEOF_POINTER__\s*(?:<=|==)\s*4|UINTPTR_MAX\s*<=\s*UINT32_MAX")

def ds_bound(build):
    names = set()
    for f in glob.glob(os.path.join(build, "ovl/boot/ov*_link.c")) + \
             glob.glob(os.path.join(build, "armdata/*.c")):
        for m in BIND.finditer(open(f, errors="replace").read()):
            names.add(m.group(1))
    return names

def scan(path, names):
    out, stack = [], []
    try:
        lines = open(path, errors="replace").read().splitlines()
    except OSError:
        return out
    for i, ln in enumerate(lines, 1):
        s = ln.strip()
        if s.startswith("#if"):
            stack.append("wide" if WIDE.search(s) else "narrow" if NARROW.search(s) else None)
            continue
        if s.startswith("#el") and stack:
            t = stack[-1]
            stack[-1] = "narrow" if t == "wide" else "wide" if t == "narrow" else None
            continue
        if s.startswith("#endif") and stack:
            stack.pop()
            continue
        if "narrow" in stack or s.startswith(("*", "//", "/*", "#")):
            continue
        for m in CALL.finditer(ln):
            if m.group(1) in names:
                out.append(dict(file=path, line=i, symbol=m.group(1),
                                member=m.group(3), snippet=s[:160]))
    return out

def main():
    build = sys.argv[1]
    names = ds_bound(build)
    hits = []
    for root in ("dsaddr", "ovl/boot/src"):
        for dp, _, fs in os.walk(os.path.join(build, root)):
            for f in fs:
                if f.endswith(".c"):
                    hits += scan(os.path.join(dp, f), names)
    for h in hits:
        print(json.dumps(h))
    syms = sorted({h["symbol"] for h in hits})
    sys.stderr.write("DS-bound symbols: %d; call sites through them: %d (%d tables)\n"
                     % (len(names), len(hits), len(syms)))
    for h in hits:
        sys.stderr.write("  %s:%d %s\n" % (h["file"].split("/src/game/")[-1].split("/dsaddr/")[-1],
                                           h["line"], h["snippet"][:100]))

if __name__ == "__main__":
    main()
