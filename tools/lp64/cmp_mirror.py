#!/usr/bin/env python3
"""Compare a port mirror struct's field offsets with the canonical game struct.

usage: cmp_mirror.py PORT_JSON PORT_STRUCT GAME_JSON GAME_STRUCT
Fields are paired by position (port mirrors rename fields); prints each pair's
offsets and flags the first divergence and every mismatch.
"""
import json, sys

p = json.load(open(sys.argv[1]))[sys.argv[2]]
g = json.load(open(sys.argv[3]))[sys.argv[4]]
if not p or not g:
    sys.exit("layout missing: port=%s game=%s" % (bool(p), bool(g)))
print("size: port %d  game %d" % (p["size"], g["size"]))
bad = 0
for i, (po, pn) in enumerate(p["fields"]):
    go, gn = g["fields"][i] if i < len(g["fields"]) else (None, "-")
    mark = "" if po == go else "   <-- MISMATCH"
    bad += po != go
    print("%4s %-30s | %4s %s%s" % (po, pn[:30], go, gn[:34], mark))
print("mismatches:", bad)
