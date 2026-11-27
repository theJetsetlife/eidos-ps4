#!/usr/bin/env bash
# Check a built game FPKG before it is handed over.
#
#   tools/ps4/verify_fpkg.sh <pkg> <elf>
#
# Fails when the package lacks eboot.bin, sce_sys/param.sfo or
# sce_sys/icon0.png, when it holds anything that looks like ROM data or an
# SDK file other than the two runtime modules the pipeline bundles, when the
# ELF is not a 64-bit x86-64 Orbis executable, or when the ELF needs a
# library the package does not carry and the console does not provide.
set -euo pipefail
die() { echo "verify_fpkg: $*" >&2; exit 1; }

PKG=${1:?pkg} ELF=${2:?elf}
OO=${OO_PS4_TOOLCHAIN:-$HOME/OpenOrbis/PS4Toolchain}
PKGTOOL=$OO/bin/linux/PkgTool.Core
[ -f "$PKG" ] || die "no package $PKG"
[ -f "$ELF" ] || die "no ELF $ELF"

LIST=$("$PKGTOOL" pkg_listentries "$PKG" 2>&1) || die "pkg_listentries failed"

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$PKGTOOL" pkg_extract "$PKG" "$T/x" >/dev/null 2>&1 || die "pkg_extract failed"
FILES=$(cd "$T/x" && find . -type f | sed 's|^\./||; s|^uroot/||' | sort)
echo "package files:"; echo "$FILES" | sed 's/^/  /'
echo "package entries:"; echo "$LIST" | awk 'NR>1 {print "  " $NF}'

# the executable is in the file system image; param.sfo and the artwork are
# package entries (the installer reads them from there)
echo "$FILES" | grep -qx 'eboot.bin' || die "package lacks eboot.bin"
for e in PARAM_SFO ICON0_PNG; do
  echo "$LIST" | grep -qw "$e" || die "package lacks the $e entry"
done

# nothing from the player's game data or the SDK's own binaries
if echo "$FILES" | grep -i -E '\.(nds|srl|dsi|3ds|nsp|xci|sav|dsv|bin\.rom)$|/extract/|rom' | grep -v -E 'sce_sys/' >/dev/null; then
  die "package holds a file that looks like ROM data"
fi
EXTRA=$(echo "$FILES" | grep -E '\.(prx|sprx|elf|self)$' | grep -v -E '(^|/)(eboot\.bin|sce_module/(libc|libSceFios2)\.prx)$' || true)
[ -z "$EXTRA" ] || die "unexpected binaries in the package: $EXTRA"

# the package's own digests
VAL=$("$PKGTOOL" pkg_validate "$PKG" 2>&1 || true)
if echo "$VAL" | grep -q -E '^\[(FAIL|ERROR)'; then
  echo "$VAL" | grep -E '^\[(FAIL|ERROR)' >&2
  die "pkg_validate reported a failure"
fi
echo "pkg_validate: $(echo "$VAL" | grep -c '^\[OK\]') checks ok"

# the executable
HDR=$(readelf -h "$ELF" 2>/dev/null || true)
echo "$HDR" | grep -q 'ELF64' || die "ELF is not 64-bit"
echo "$HDR" | grep -q 'X86-64' || die "ELF is not x86-64"
echo "$HDR" | grep -q 'FreeBSD' || die "ELF is not an Orbis (FreeBSD ABI) executable"

# its needed libraries: bundled in the package, or provided by the console
CONSOLE_LIBS='^(libkernel|libkernel_sys|libSce[A-Za-z0-9_]*|libScePad|libc|libSceLibcInternal)(\.|$)'
NEEDED=$(readelf -d "$ELF" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' || true)
if [ -n "$NEEDED" ]; then
  echo "needed libraries:"; echo "$NEEDED" | sed 's/^/  /'
  for n in $NEEDED; do
    b=${n%.prx}; b=${b%.sprx}
    if echo "$FILES" | grep -q "sce_module/$b\.prx"; then continue; fi
    echo "$b" | grep -E "$CONSOLE_LIBS" >/dev/null || die "$n is neither bundled nor a console library"
  done
else
  echo "needed libraries: none listed by readelf (Orbis dynamic info is read by the console)"
fi
echo "verify_fpkg: ok"
