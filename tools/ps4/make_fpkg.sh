#!/usr/bin/env bash
# Build an installable test PKG from an SDK-linked Orbis ELF.
#
#   ELF (ET_SCE_DYNEXEC) -> fake-signed eboot.bin -> param.sfo + icon -> gp4 -> PKG
#
# The PKG installs on a console that accepts fake-signed packages (a homebrew-
# enabled retail unit or a development unit in the matching mode). It is a
# test package, not a Sony-signed publishing build.
#
# Usage:
#   tools/ps4/make_fpkg.sh --elf build/ps4-diagnostics/eidos_ps4_minimal.elf \
#       --title "Eidos Platform Diagnostic" --title-id EIDP90001 --label DIAGNOSTIC000001 \
#       [--version 01.00] [--icon icon.png] [--pic1 background.png] [--module libc.prx ...] \
#       [--sdk-version 0x09008001] [--out out/pkg]
#
# Environment:
#   OO_PS4_TOOLCHAIN   OpenOrbis toolchain root (PkgTool.Core, create-gp4)
#   SCE_ORBIS_SDK_DIR  SDK root; --module NAME is looked up in target/sce_module
#   SOURCE_DATE_EPOCH  reproducible package timestamp (default: source HEAD)
#
# Nothing from the SDK, ROMs or extracted data is written to the source tree;
# all staging is under --out.
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
die() { echo "make_fpkg: $*" >&2; exit 1; }

SOURCE_ROOT=$(cd "$HERE/../.." && pwd)
PACKAGE_EPOCH=${SOURCE_DATE_EPOCH:-$(git -C "$SOURCE_ROOT" log -1 --format=%ct 2>/dev/null || true)}
[[ $PACKAGE_EPOCH =~ ^[0-9]+$ ]] || die "SOURCE_DATE_EPOCH must be an integer Unix timestamp (or run from a Git checkout)"
PACKAGE_DATETIME=$(date -u -d "@$PACKAGE_EPOCH" '+%Y-%m-%d %H:%M:%S') || die "invalid SOURCE_DATE_EPOCH: $PACKAGE_EPOCH"
PACKAGE_ISO8601=$(date -u -d "@$PACKAGE_EPOCH" '+%Y-%m-%dT%H:%M:%SZ') || die "invalid SOURCE_DATE_EPOCH: $PACKAGE_EPOCH"

ELF= TITLE= TITLE_ID= LABEL= VERSION=01.00 ICON= PIC1= OUT= SDKVER= PAID=0x3800000000000011
MODULES=()
while [ $# -gt 0 ]; do
  case $1 in
    --elf) ELF=$2; shift 2;;
    --title) TITLE=$2; shift 2;;
    --title-id) TITLE_ID=$2; shift 2;;
    --label) LABEL=$2; shift 2;;
    --version) VERSION=$2; shift 2;;
    --icon) ICON=$2; shift 2;;
    --pic1) PIC1=$2; shift 2;;
    --module) MODULES+=("$2"); shift 2;;
    --sdk-version) SDKVER=$2; shift 2;;
    --paid) PAID=$2; shift 2;;
    --out) OUT=$2; shift 2;;
    -h|--help) sed -n 2,22p "$0"; exit 0;;
    *) die "unknown option $1";;
  esac
done

[ -f "$ELF" ] || die "--elf missing or not a file"
ELF=$(readlink -f "$ELF")
[ -n "$TITLE" ] || die "--title required"
[[ $TITLE != *\'* && $TITLE != *\"* ]] || die "title must not contain quotes"
[[ $TITLE_ID =~ ^[A-Z]{4}[0-9]{5}$ ]] || die "--title-id must be 4 capital letters + 5 digits"
[[ $LABEL =~ ^[A-Z0-9]{16}$ ]] || die "--label must be 16 capital letters/digits"
[[ $VERSION =~ ^[0-9]{2}\.[0-9]{2}$ ]] || die "--version must look like 01.00"
case $TITLE_ID in CUSA*|PCAS*|PLAS*|PCJS*|PLJS*|PCKS*|PLKS*|NPXS*|CUSB*) die "title ID prefix $TITLE_ID is used by real titles/system apps; pick another";; esac

OO=${OO_PS4_TOOLCHAIN:-$HOME/OpenOrbis/PS4Toolchain}
PKGTOOL=$OO/bin/linux/PkgTool.Core
CREATE_GP4=$OO/bin/linux/create-gp4
[ -x "$PKGTOOL" ] || die "PkgTool.Core not found under $OO (set OO_PS4_TOOLCHAIN)"
[ -x "$CREATE_GP4" ] || die "create-gp4 not found under $OO"
[ -f "$HERE/third_party/make_fself_py3.py" ] || OO_PS4_TOOLCHAIN=$OO "$HERE/port_make_fself.sh" >/dev/null

CONTENT_ID="IV0000-${TITLE_ID}_00-${LABEL}"
OUT=${OUT:-$PWD/out/pkg/$TITLE_ID}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
ST=$OUT/stage
rm -rf "$ST"; mkdir -p "$ST/sce_sys" "$ST/sce_module"
PY="python3 -I -W ignore"

# --- executable: optional SDK-version patch on a copy, then fake-sign
SRC_ELF=$ELF
if [ -n "$SDKVER" ]; then
  SRC_ELF=$OUT/eboot.patched.elf
  $PY "$HERE/patch_sdk_version.py" "$ELF" "$SRC_ELF" "$SDKVER"
fi
$PY "$HERE/third_party/make_fself_py3.py" --paid "$PAID" "$SRC_ELF" "$ST/eboot.bin" >/dev/null
$PY "$HERE/check_fself.py" --expect-paid "$PAID" "$SRC_ELF" "$ST/eboot.bin"

# --- modules: SDK PRX files the ELF links against, fake-signed into sce_module/
for m in "${MODULES[@]}"; do
  p=$m
  [ -f "$p" ] || p=${SCE_ORBIS_SDK_DIR:-}/target/sce_module/$m
  [ -f "$p" ] || die "module $m not found (give a path or set SCE_ORBIS_SDK_DIR)"
  n=$(basename "$p")
  if [ "$(head -c 4 "$p" | od -An -tx1 | tr -d ' ')" = 7f454c46 ]; then
    $PY "$HERE/third_party/make_fself_py3.py" --paid "$PAID" "$p" "$ST/sce_module/$n" >/dev/null
    $PY "$HERE/check_fself.py" "$p" "$ST/sce_module/$n" | tail -1 | sed "s|^|  $n: |"
  else
    cp "$p" "$ST/sce_module/$n"   # already a SELF
  fi
done

# --- icon (512x512 24-bit PNG)
if [ -n "$ICON" ]; then
  magick "$ICON" -resize 512x512! -depth 8 -type TrueColor -define png:color-type=2 "$ST/sce_sys/icon0.png"
else
  magick -size 512x512 xc:'#1d2430' -fill '#e8edf3' -gravity center -font DejaVu-Sans-Bold \
    -pointsize 88 -annotate +0-60 'PS4' -pointsize 52 -annotate +0+40 'Platform' \
    -annotate +0+110 'Diagnostic' -depth 8 -type TrueColor -define png:color-type=2 "$ST/sce_sys/icon0.png"
fi

# --- background artwork (1920x1080 24-bit PNG), optional
if [ -n "$PIC1" ]; then
  magick "$PIC1" -resize 1920x1080! -depth 8 -type TrueColor -define png:color-type=2 "$ST/sce_sys/pic1.png"
fi

# --- param.sfo
S=$ST/sce_sys/param.sfo
"$PKGTOOL" sfo_new "$S"
set_sfo() { "$PKGTOOL" sfo_setentry "$S" "$1" --type "$2" --maxsize "$3" --value "$4" >/dev/null; }
set_sfo APP_TYPE Integer 4 1
set_sfo APP_VER Utf8 8 "$VERSION"
set_sfo ATTRIBUTE Integer 4 0
set_sfo CATEGORY Utf8 4 gd
set_sfo CONTENT_ID Utf8 48 "$CONTENT_ID"
set_sfo DOWNLOAD_DATA_SIZE Integer 4 0
set_sfo SYSTEM_VER Integer 4 0
set_sfo TITLE Utf8 128 "$TITLE"
set_sfo TITLE_ID Utf8 12 "$TITLE_ID"
set_sfo VERSION Utf8 8 "$VERSION"

# create-gp4 and PkgTool preserve input metadata in the package. Normalize the
# complete stage after all generated files exist so equal inputs yield equal
# FPKG bytes.
find "$ST" -type f -exec touch -d "@$PACKAGE_EPOCH" {} +

# --- gp4 + pkg
cd "$ST"
FILES="eboot.bin sce_sys/param.sfo sce_sys/icon0.png"
[ -f sce_sys/pic1.png ] && FILES="$FILES sce_sys/pic1.png"
for f in sce_module/*; do [ -e "$f" ] && FILES="$FILES $f"; done
"$CREATE_GP4" -out pkg.gp4 --content-id="$CONTENT_ID" --files "$FILES" >/dev/null
# create-gp4 has no timestamp option and always writes the wall clock into
# volume_ts. Replace that one generated field before PkgTool hashes the GP4.
sed -i "s#<volume_ts>[^<]*</volume_ts>#<volume_ts>$PACKAGE_DATETIME</volume_ts>#" pkg.gp4
grep -q "<volume_ts>$PACKAGE_DATETIME</volume_ts>" pkg.gp4 || die "failed to normalize GP4 volume timestamp"
"$PKGTOOL" pkg_build pkg.gp4 "$OUT" >/dev/null
PKG=$OUT/$CONTENT_ID.pkg
[ -f "$PKG" ] || die "PkgTool did not produce $PKG"
"$PKGTOOL" pkg_validate "$PKG" > "$OUT/validate.txt" 2>&1 || true

{
  echo "pkg=$PKG"
  echo "bytes=$(stat -c %s "$PKG")"
  echo "sha256=$(sha256sum "$PKG" | cut -d' ' -f1)"
  echo "content_id=$CONTENT_ID"
  echo "title=$TITLE"
  echo "version=$VERSION"
  echo "source_elf=$(readlink -f "$ELF")"
  echo "source_elf_sha256=$(sha256sum "$ELF" | cut -d' ' -f1)"
  echo "sdk_version_patch=${SDKVER:-none}"
  echo "modules=${MODULES[*]:-none}"
  echo "built=$PACKAGE_ISO8601"
} > "$OUT/manifest.txt"
cat "$OUT/manifest.txt"
