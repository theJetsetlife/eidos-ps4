#!/usr/bin/env bash
# Build the installable Eidos game package from the SDK-linked Orbis ELF.
#
#   tools/ps4/make_game_pkg.sh [--official] [--elf path] [--out dir] [--version 01.00]
#
# Default ELF: build/ps4-game-debug/eidos_boot.elf
# Default out: build/pkg/game-fpkg (fake-signed) or build/pkg/game-official
#
# The package carries the executable, the two SDK runtime modules the
# pipeline bundles, param.sfo, icon0.png and pic1.png.  Shaders are compiled
# into the executable.  It never carries ROM data; the player's ROM is
# imported from a USB drive at run time.  The Title ID stays EIDP90002 for
# every release so a newer package installs over the old one and the save
# data (keyed by Title ID) is kept.  Bump --version for an update.
#
# The fake-signed package installs on a console running GoldHEN.  The run
# log (EIDOS_LOG.txt) goes to a USB drive in the console (engine/platform/
# ps4/log_ps4.h).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
die() { echo "make_game_pkg: $*" >&2; exit 1; }

OFFICIAL=0
ELF=$ROOT/build/ps4-game-debug/eidos_boot.elf
OUT=
VERSION=01.00
while [ $# -gt 0 ]; do
  case $1 in
    --official) OFFICIAL=1; shift;;
    --elf) ELF=$2; shift 2;;
    --out) OUT=$2; shift 2;;
    --version) VERSION=$2; shift 2;;
    -h|--help) sed -n 2,17p "$0"; exit 0;;
    *) die "unknown option $1";;
  esac
done

[ -f "$ELF" ] || die "game ELF not found at $ELF"

TITLE_ID=EIDP90002
LABEL=EIDOSGAME0000001
TITLE="Eidos"

if [ "$OFFICIAL" = 1 ]; then
  OUT=${OUT:-$ROOT/build/pkg/game-official}
  exec bash "$HERE/make_pkg_official.sh" --elf "$ELF" --title "$TITLE" \
    --title-id "$TITLE_ID" --label "$LABEL" --out "$OUT"
fi

OUT=${OUT:-$ROOT/build/pkg/game-fpkg}
mkdir -p "$OUT"
ART=$OUT/art
mkdir -p "$ART"
# Artwork drawn here (no third-party image): replace by passing real files
# through EIDOS_ICON / EIDOS_PIC1.
if [ -n "${EIDOS_ICON:-}" ]; then ICON=$EIDOS_ICON; else
  ICON=$ART/icon0.png
  magick -size 512x512 gradient:'#16213e-#0f3460' -fill '#e8edf3' -gravity center \
    -font DejaVu-Sans-Bold -pointsize 120 -annotate +0-20 'Eidos' \
    -pointsize 36 -annotate +0+90 'for PlayStation 4' "$ICON"
fi
if [ -n "${EIDOS_PIC1:-}" ]; then PIC1=$EIDOS_PIC1; else
  PIC1=$ART/pic1.png
  magick -size 1920x1080 gradient:'#0f3460-#16213e' "$PIC1"
fi

bash "$HERE/make_fpkg.sh" --elf "$ELF" --title "$TITLE" --title-id "$TITLE_ID" \
  --label "$LABEL" --version "$VERSION" --icon "$ICON" --pic1 "$PIC1" \
  --module libc.prx --module libSceFios2.prx --out "$OUT"

PKG=$OUT/IV0000-${TITLE_ID}_00-${LABEL}.pkg
bash "$HERE/verify_fpkg.sh" "$PKG" "$ELF" | tee "$OUT/verify.txt"
echo "game package: $PKG"
