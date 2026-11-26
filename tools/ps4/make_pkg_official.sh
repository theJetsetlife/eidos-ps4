#!/usr/bin/env bash
# Build a PKG with the official SDK Publishing Tools (orbis-pub-cmd 4.12,
# shipped with SDK 12.508.041), then verify it with img_verify.
#
#   tools/ps4/make_pkg_official.sh --elf build/.../eidos_ps4_minimal.elf \
#       --title "Eidos Platform Diagnostic" --title-id EIDP90001 \
#       --label DIAGNOSTIC000001 [--version 01.00] [--icon icon.png] \
#       [--pic pic1.png] [--out build/pkg/diagnostic-official]
#
# The ELF goes into the package as eboot.bin, as the SDK documents. libc.prx
# and libSceFios2.prx come from $SCE_ORBIS_SDK_DIR/target/sce_module.
#
# Environment:
#   SCE_ORBIS_SDK_DIR     SDK 12.508.041 root (host_tools, target)
#   SCE_PUBTOOLS_DIR      Publishing Tools directory holding orbis_pub_cmd.exe
#                         and ext/sc.exe (default: $SCE_ORBIS_SDK_DIR/../publishing-tools-4.12)
#   EIDOS_WINE            wine binary (default: Proton Experimental)
#   EIDOS_WINEPREFIX      prefix (default: /tmp/eidos-orbis-wine)
set -euo pipefail
die() { echo "make_pkg_official: $*" >&2; exit 1; }

ELF= TITLE= TITLE_ID= LABEL= VERSION=01.00 ICON= PIC= OUT=
PASSCODE=00000000000000000000000000000000
while [ $# -gt 0 ]; do
  case $1 in
    --elf) ELF=$2; shift 2;;
    --title) TITLE=$2; shift 2;;
    --title-id) TITLE_ID=$2; shift 2;;
    --label) LABEL=$2; shift 2;;
    --version) VERSION=$2; shift 2;;
    --icon) ICON=$2; shift 2;;
    --pic) PIC=$2; shift 2;;
    --passcode) PASSCODE=$2; shift 2;;
    --out) OUT=$2; shift 2;;
    -h|--help) sed -n 2,19p "$0"; exit 0;;
    *) die "unknown option $1";;
  esac
done
[ -f "$ELF" ] || die "--elf missing"; ELF=$(readlink -f "$ELF")
[[ $TITLE_ID =~ ^[A-Z]{4}[0-9]{5}$ ]] || die "--title-id must be 4 capital letters + 5 digits"
[[ $LABEL =~ ^[A-Z0-9]{16}$ ]] || die "--label must be 16 capital letters/digits"
[[ $VERSION =~ ^[0-9]{2}\.[0-9]{2}$ ]] || die "--version must look like 01.00"
[[ $PASSCODE =~ ^[A-Za-z0-9_-]{32}$ ]] || die "--passcode must be 32 characters"
[ -n "$TITLE" ] && [[ $TITLE != *[\<\>\&\"\']* ]] || die "--title required, without XML special characters"
case $TITLE_ID in CUSA*|PCAS*|PLAS*|PCJS*|PLJS*|PCKS*|PLKS*|NPXS*|CUSB*) die "title ID prefix is used by real titles/system apps";; esac

SDK=${SCE_ORBIS_SDK_DIR:?set SCE_ORBIS_SDK_DIR to the SDK 12.508.041 root}
SDK=$(readlink -f "$SDK")
grep -q '0x12508041u' "$SDK/target/include/sdk_version.h" || die "SDK at $SDK is not 12.508.041"
PUB=${SCE_PUBTOOLS_DIR:-$(dirname "$(readlink -f "$SDK/target")")/../publishing-tools-4.12}
PUB=$(readlink -f "$PUB")
[ -f "$PUB/orbis_pub_cmd.exe" ] || die "orbis_pub_cmd.exe not found in $PUB (set SCE_PUBTOOLS_DIR)"
[ -f "$PUB/ext/sc.exe" ] || die "$PUB/ext/sc.exe missing; Publishing Tools need the MSI's ext/ directory"
WINE=${EIDOS_WINE:-"$HOME/.steam/steam/steamapps/common/Proton - Experimental/files/bin/wine"}
[ -x "$WINE" ] || die "wine not found at $WINE (set EIDOS_WINE)"
export WINEPREFIX=${EIDOS_WINEPREFIX:-/tmp/eidos-orbis-wine} WINEDEBUG=-all

CONTENT_ID="IV0000-${TITLE_ID}_00-${LABEL}"
OUT=${OUT:-$PWD/out/pkg-official/$TITLE_ID}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
ST=$OUT/stage
rm -rf "$ST" "$OUT/out" "$OUT/project.gp4"; mkdir -p "$ST/sce_sys" "$ST/sce_module" "$OUT/out"
LOG=$OUT/publishing.log; : > "$LOG"

pub() {
  local rc=0
  (cd "$OUT" && timeout 900 "$WINE" "$PUB/orbis_pub_cmd.exe" "$@") >> "$LOG" 2>&1 || rc=$?
  return $rc
}

# --- contents
cp "$ELF" "$ST/eboot.bin"
for m in libc.prx libSceFios2.prx; do
  [ -f "$SDK/target/sce_module/$m" ] || die "$m missing from SDK target/sce_module"
  cp "$SDK/target/sce_module/$m" "$ST/sce_module/$m"
done
if [ -n "$ICON" ]; then
  magick "$ICON" -resize 512x512! -depth 8 -type TrueColor -define png:color-type=2 "$ST/sce_sys/icon0.png"
else
  magick -size 512x512 xc:'#1d2430' -fill '#e8edf3' -gravity center -font DejaVu-Sans-Bold \
    -pointsize 88 -annotate +0-60 'PS4' -pointsize 52 -annotate +0+40 'Platform' \
    -annotate +0+110 'Diagnostic' -depth 8 -type TrueColor -define png:color-type=2 "$ST/sce_sys/icon0.png"
fi
if [ -n "$PIC" ]; then
  magick "$PIC" -resize 1920x1080! -depth 8 -type TrueColor -define png:color-type=2 "$ST/sce_sys/pic1.png"
else
  magick -size 1920x1080 gradient:'#2a3444-#10151d' -depth 8 -type TrueColor -define png:color-type=2 "$ST/sce_sys/pic1.png"
fi

# --- param.sfo (ATTRIBUTE 0x2: common dialogs confirm with Cross)
cat > "$OUT/param.sfx" <<EOF
<?xml version="1.0" encoding="utf-8" standalone="yes"?>
<paramsfo>
  <param key="FORMAT">obs</param>
  <param key="APP_TYPE">0</param>
  <param key="SYSTEM_VER">0</param>
  <param key="CATEGORY">gd</param>
  <param key="CONTENT_ID">$CONTENT_ID</param>
  <param key="TITLE_ID">$TITLE_ID</param>
  <param key="VERSION">$VERSION</param>
  <param key="APP_VER">$VERSION</param>
  <param key="PARENTAL_LEVEL">1</param>
  <param key="TITLE">$TITLE</param>
  <param key="ATTRIBUTE">2</param>
  <param key="DOWNLOAD_DATA_SIZE">0</param>
  <param key="REMOTE_PLAY_KEY_ASSIGN">0</param>
</paramsfo>
EOF
pub sfo_create param.sfx stage/sce_sys/param.sfo || die "sfo_create failed (see $LOG)"
[ -s "$ST/sce_sys/param.sfo" ] || die "sfo_create wrote no param.sfo (see $LOG)"

# --- project
pub gp4_proj_create --volume_type pkg_ps4_app --content_id "$CONTENT_ID" --passcode "$PASSCODE" \
    --storage_type digital50 --app_type full --c_date actual_datetime project.gp4 || die "gp4_proj_create failed (see $LOG)"
for f in eboot.bin sce_module/libc.prx sce_module/libSceFios2.prx sce_sys/param.sfo sce_sys/icon0.png sce_sys/pic1.png; do
  pub gp4_file_add "stage/$f" "$f" project.gp4 || die "gp4_file_add $f failed (see $LOG)"
done

# --- image + verification
pub img_create --oformat pkg --no_progress_bar project.gp4 out || true
PKG=$(ls "$OUT"/out/*.pkg 2>/dev/null | head -1)
[ -n "$PKG" ] || die "img_create produced no PKG (see $LOG)"
grep -q "Create image Process finished with error" "$LOG" && die "img_create reported errors (see $LOG)"
VER=$OUT/verify.txt
(cd "$OUT" && timeout 900 "$WINE" "$PUB/orbis_pub_cmd.exe" img_verify --passcode "$PASSCODE" "out/$(basename "$PKG")") > "$VER" 2>&1 || true
ERRS=$(grep -c '^\[Error\]' "$VER" || true)
VERDICT=$(grep -o 'Result : [A-Z]*' "$VER" | tail -1)
(cd "$OUT" && timeout 300 "$WINE" "$PUB/orbis_pub_cmd.exe" img_file_list --passcode "$PASSCODE" "out/$(basename "$PKG")") 2>/dev/null \
  | grep -E '^(Image0|Sc0)/' > "$OUT/file_list.txt" || true

{
  echo "pkg=$PKG"
  echo "bytes=$(stat -c %s "$PKG")"
  echo "sha256=$(sha256sum "$PKG" | cut -d' ' -f1)"
  echo "content_id=$CONTENT_ID"
  echo "passcode=$PASSCODE"
  echo "title=$TITLE"
  echo "version=$VERSION"
  echo "tool=orbis-pub-cmd 4.12 (Publishing Tools), SDK 12.508.041"
  echo "eboot=SDK ELF, unmodified"
  echo "source_elf=$ELF"
  echo "source_elf_sha256=$(sha256sum "$ELF" | cut -d' ' -f1)"
  echo "img_verify=${VERDICT:-unknown} (errors: $ERRS)"
  echo "built=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$OUT/manifest.txt"
cat "$OUT/manifest.txt"
grep -E '^\[(Error|Warn)\]' "$VER" | grep -v -E 'online check|Number of|Result :|Process finished' | sed 's/^/  /' || true
[ "$ERRS" = 0 ] || [ "$VERDICT" != "Result : ERROR" ] || exit 1
