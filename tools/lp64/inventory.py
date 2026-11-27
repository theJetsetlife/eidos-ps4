#!/usr/bin/env python3
"""LP64 defect inventory: classify detector candidates, keep corrected history.

    python tools/lp64/detect_ptr_stride.py build-host-boot/dsaddr \
        build-host-boot/ovl/boot/src stubs sdk engine > /tmp/stride.jsonl
    python tools/lp64/inventory.py /tmp/stride.jsonl > tools/lp64/inventory.jsonl

Each line: id, class, file, line, func, status (corrected | confirmed |
probable | deferred | valid), reach (critical | gameplay | online | boot),
evidence, fix, commit.  Pattern hits are candidates, never auto-rewritten.
"""
import json, re, sys

# v0.1 scope (docs: online services deferred).  Overlay id -> reachability.
ONLINE = {9, 16, 97, 100, 114, 119, 120, 135, 172, 177}
GAMEPLAY = {70, 112, 132, 208, 225}
CRITICAL = {10, 21, 93, 95, 197, 198, 204}   # field, scripts, battle (+effects,
                                             # bag, party), intro

CORRECTED = [
    ("convert.c GAME_TYPES_U32_LONG", "type-width", "1ead0d0",
     "u32=unsigned long (8B) in one TU broke RTCDate cross-TU layout; strip define in convert.c only"),
    ("nvram.c OSMessage receive", "narrow-store", "f491d76",
     "8-byte OSMessage received into 4-byte NVRAMResult; receive into OSMessage local"),
    ("nns_fnd_expheap MemRegion", "pointer-in-int", "f491d76",
     "region pair held addresses in int; widen to sptr"),
    ("ov021 GetFrontTileMessage table", "ds-fnptr-table", "1b0b980",
     "DS fn-ptr table indexed with LP64 stride; read 8B DS entries, resolve via bw_ovl_code_entry"),
    ("ov021 data_021D3CE8 binding", "stale-dsref", "445bfb3",
     "bw_dsref global stale in opening scene; resolve with bw_ds_try_ptr"),
    ("ov021 cut prototypes", "return-width", "c30f3d9",
     "--cut-decls dropped u16 prototype -> implicit int return; patch header 'keeps declarations'"),
    ("mod_event_battle BwPortSetup", "port-mirror-layout", "422197a",
     "netHandle/mapMatrix u32 vs game pointers shifted fields; void*, verified 0 mismatches via abi_dump"),
    ("btl_setup BTL_SETUP_SetTrainer", "ptr-array-stride", "4b20ed5",
     "off=client*4 into trainerData[] with 8B store; sizeof(struct BattleTrainerData *)"),
    ("calls through DS-bound fn-pointer arrays (94 sites)", "ds-fnptr-table", "60209fd",
     "dsaddr.py rewrites TABLE[i](..)/TABLE[i][j](..) through BIND_DS names to "
     "bw_ds_code_word (4-byte DS word, resolved); overlays now get --pointer-width"),
    ("PM_VOICE_Stop in sdk/cries.c", "native-handle-in-sidecar", "e3e92dd",
     "START@4800 crashed at frame 4929 when two DS channel-presence words were read as a native handle; game-tree cleanup now uses Aletheia's channel accessors"),
]

def reach(path):
    m = re.search(r"/ov(\d{3})/", path)
    if not m:
        return "boot"
    n = int(m.group(1))
    if n in CRITICAL:
        return "critical"
    if n in ONLINE:
        return "online"
    if n in GAMEPLAY:
        return "gameplay"
    return "gameplay"

def main():
    rows = []
    for i, (what, cls, commit, ev) in enumerate(CORRECTED, 1):
        rows.append(dict(id="C%02d" % i, cls=cls, what=what, status="corrected",
                         commit=commit, evidence=ev))
    for n, ln in enumerate(open(sys.argv[1]), 1):
        h = json.loads(ln)
        r = reach(h["file"])
        rows.append(dict(
            id="P%03d" % n, cls="ptr-array-stride", file=h["file"].split("/src/game/")[-1],
            line=h["line"], func=h.get("func"), reach=r,
            status="deferred" if r == "online" else "probable",
            evidence="pattern %s: %s" % (h["pattern"], h["snippet"][:120]),
            fix="stride *4 -> *sizeof(ptr) AND verify the DS base offset against "
                "the struct's LP64 layout (abi_dump) before rewriting"))
    # Optional second input: detect_ds_fntable.py hits.  After the dsaddr
    # rewrite only struct tables (`TABLE[i].fn(...)`) remain: they need the
    # element's DS layout (abi_dump with -m32) before they can be converted.
    if len(sys.argv) > 2:
        for n, ln in enumerate(open(sys.argv[2]), 1):
            h = json.loads(ln)
            r = reach(h["file"])
            rows.append(dict(
                id="F%03d" % n, cls="ds-fnptr-struct-table",
                file=h["file"].split("/src/game/")[-1].split("/dsaddr/")[-1],
                line=h["line"], symbol=h["symbol"], member=h.get("member"), reach=r,
                status="deferred" if r == "online" else "probable",
                evidence=h["snippet"][:120],
                fix="read the member's 4-byte DS word at i*DS_ELEM+DS_OFF "
                    "(ILP32 layout via abi_dump -m32) and resolve with bw_ds_code_word"))
    for row in rows:
        print(json.dumps(row))
    by = {}
    for row in rows:
        by[row["status"]] = by.get(row["status"], 0) + 1
    sys.stderr.write("inventory: %s\n" % by)

if __name__ == "__main__":
    main()
