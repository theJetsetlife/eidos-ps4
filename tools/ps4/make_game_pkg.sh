#!/usr/bin/env bash
# Convenience wrapper: build an installable game application PKG from the
# SDK-linked Orbis game ELF.
#
#   tools/ps4/make_game_pkg.sh [--official] [--elf path] [--out dir]
#
# Default ELF: build/ps4-game-debug/eidos_boot.elf
# Default out:  build/pkg/game-fpkg (fake) or build/pkg/game-official (official)
#
# Environment: same as make_fpkg.sh / make_pkg_official.sh.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
die() { echo "make_game_pkg: $*" >&2; exit 1; }

OFFICIAL=0
ELF=$ROOT/build/ps4-game-debug/eidos_boot.elf
OUT=
while [ $# -gt 0 ]; do
  case $1 in
    --official) OFFICIAL=1; shift;;
    --elf) ELF=$2; shift 2;;
    --out) OUT=$2; shift 2;;
    -h|--help) sed -n 2,10p "$0"; exit 0;;
    *) die "unknown option $1";;
  esac
done

[ -f "$ELF" ] || die "game ELF not found at $ELF"

TITLE_ID=EIDP90002
LABEL=EIDOSGAME0000001
TITLE="Eidos"

COMMON_ARGS=(
  --elf "$ELF"
  --title "$TITLE"
  --title-id "$TITLE_ID"
  --label "$LABEL"
)

if [ "$OFFICIAL" = 1 ]; then
  OUT=${OUT:-$ROOT/build/pkg/game-official}
  exec bash "$HERE/make_pkg_official.sh" "${COMMON_ARGS[@]}" --out "$OUT"
else
  OUT=${OUT:-$ROOT/build/pkg/game-fpkg}
  exec bash "$HERE/make_fpkg.sh" "${COMMON_ARGS[@]}" \
    --module libc.prx --module libSceFios2.prx --out "$OUT"
fi
