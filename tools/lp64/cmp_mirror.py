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
bad = int(p["size"] != g["size"])
if len(p["fields"]) != len(g["fields"]):
    bad += 1
    print("field count: port %d  game %d   <-- MISMATCH" %
          (len(p["fields"]), len(g["fields"])))
if "alignment" in p or "alignment" in g:
    if p.get("alignment") is None or g.get("alignment") is None or p["alignment"] != g["alignment"]:
        bad += 1
        print("alignment: port %s  game %s   <-- MISMATCH" %
              (p.get("alignment"), g.get("alignment")))
for i, (po, pn) in enumerate(p["fields"]):
    go, gn = g["fields"][i] if i < len(g["fields"]) else (None, "-")
    mark = "" if po == go else "   <-- MISMATCH"
    bad += po != go
    print("%4s %-30s | %4s %s%s" % (po, pn[:30], go, gn[:34], mark))
print("mismatches:", bad)

sys.exit(1 if bad else 0)
