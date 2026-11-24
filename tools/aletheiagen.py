#!/usr/bin/env python3
"""aletheiagen.py -- the game bytes some src/game files include.

A few src/game files lay out data words inside their code (crt0.c's
microcode, sha1.c's round constants, a vertex table in ov119, the DSi key
block) and take them from `build/gen/<name>.inc` under the aletheia
checkout, which aletheia's tools/gendata.py writes from the player's own
extracted ROM (the bytes never go into a repository).  The port runs the
same generator before it compiles src/game, with the regions read from the
extract directory the build already uses (the parent of BW_DATA_ROOT), so a
checkout without its own extract/ works too.

    aletheiagen.py ALETHEIA_ROOT EXTRACT_DIR

Writes only what is missing; prints what it wrote.
"""

from __future__ import annotations

import json
import os
import sys


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    root, extract = (os.path.abspath(p) for p in sys.argv[1:3])
    tools = os.path.join(root, "tools")
    if not os.path.exists(os.path.join(tools, "gendata.py")):
        print("aletheiagen.py: %s has no tools/gendata.py; nothing to do" % root)
        return
    sys.path.insert(0, tools)
    import match      # aletheia's
    import gendata    # aletheia's
    out_dir = os.path.join(root, "build", "gen")
    todo = [n for n in sorted(gendata.SPECS)
            if not os.path.exists(os.path.join(out_dir, n + ".inc"))]
    if not todo:
        return
    with open(os.path.join(root, "layout", "layout.json"), encoding="utf-8") as fh:
        layout = json.load(fh)
    regions = match.load_regions(layout, extract, overlays=True)
    match._REGION_CACHE["regions"] = regions
    available = {r.name for r in regions}
    for n in todo:
        addr, size, region = gendata.SPECS[n]
        # A normal user ROM extraction has encrypted arm9i.bin but not the
        # keyed DSi-mode arm9i_dec.bin.  Eidos does not compile Aletheia's TWL
        # card-ROM unit, so its embedded key table is not part of the native
        # boot or overlay closure.  Leave that include absent: a future target
        # that really adds the unit will fail at its #include instead of
        # receiving invented bytes.  All regions used by the current closure
        # remain mandatory.
        if region == "twl" and region not in available:
            print("aletheiagen.py: skip %s (no keyed DSi TWL extraction; "
                  "not used by the native closure)" % n)
            continue
        print("aletheiagen.py: %s" % gendata.generate(n, addr, size, region,
                                                      os.path.join(out_dir, n + ".inc")))


if __name__ == "__main__":
    main()
