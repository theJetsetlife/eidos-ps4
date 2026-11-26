#!/usr/bin/env bash
# Generate a Python 3 copy of the OpenOrbis toolchain's make_fself.py (a
# Python 2 script) at tools/ps4/third_party/make_fself_py3.py. The copy is
# gitignored; this script records the exact edits.
set -euo pipefail
OO=${OO_PS4_TOOLCHAIN:-$HOME/OpenOrbis/PS4Toolchain}
SRC=$OO/scripts/make_fself.py
DST=$(cd "$(dirname "$0")" && pwd)/third_party/make_fself_py3.py
[ -f "$SRC" ] || { echo "port_make_fself: $SRC not found (set OO_PS4_TOOLCHAIN)" >&2; exit 1; }
mkdir -p "$(dirname "$DST")"
sed -e "s/MAGIC = '\\\\x7FELF'/MAGIC = b'\\\\x7FELF'/" \
    -e "s/MAGIC = '\\\\x4F\\\\x15\\\\x3D\\\\x1D'/MAGIC = b'\\\\x4F\\\\x15\\\\x3D\\\\x1D'/" \
    -e 's/\bxrange(/range(/g' \
    -e "s/val = val.decode('hex')/val = bytes.fromhex(val)/" \
    -e "s/re.sub('\\\\s+'/re.sub(r'\\\\s+'/" \
    -e "s/.ljust(SIGNATURE_SIZE, '\\\\0')/.ljust(SIGNATURE_SIZE, b'\\\\0')/" \
    -e "s/= '\\\\0' \* /= b'\\\\0' * /" "$SRC" > "$DST.tmp"
mv "$DST.tmp" "$DST"
echo "$DST"
