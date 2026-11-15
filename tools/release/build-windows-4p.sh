#!/usr/bin/env bash
# build-windows-4p.sh: the four-player split screen's Windows download, one
# portable zip.
#
#   TAKEDAWN_UPSTREAM_WIN=<folder> tools/release/build-windows-4p.sh
#
#   dist/<version>/Takedawn-<version>-windows-x64-portable.zip
#   dist/<version>/SHA256SUMS
#
# Only the North America disc's program has the four-player code
# (docs/split4.md), so only that program is built here: tools/release/gen.sh
# and build-windows.sh run with TAKEDAWN_PKG_REGIONS=NTSC-U. Everything else
# in the folder is the upstream release's, unchanged, from an installed (or
# extracted) Takedawn 0.3.1 for Windows named by TAKEDAWN_UPSTREAM_WIN: the
# two European discs' programs (two players, as upstream), the setup window
# with its Qt files, SDL2.dll, the upscaler, the shaders and the licences.
#
# The zip is portable: portable.txt beside Takedawn.exe keeps the saves,
# settings, shader cache and game data in data\ next to it, so it never
# shares a save folder with an installed Takedawn. The setup window installs
# the data from the player's own disc on first start, as upstream.
#
# Before running: tools/release/gen.sh with TAKEDAWN_PKG_REGIONS=NTSC-U (the
# regen with the four-player marks), and the toolkit checkout with
# patches/xboxrecomp-4p.patch applied (patches/apply-toolkit.sh).
set -euo pipefail
export TAKEDAWN_PKG_REGIONS=NTSC-U
source "$(dirname "$0")/common.sh"

UP="${TAKEDAWN_UPSTREAM_WIN:?TAKEDAWN_UPSTREAM_WIN: the folder of an installed Takedawn 0.3.1 for Windows}"
UP_VERSION=0.3.1
for f in Takedawn-pal.exe Takedawn-ensv.exe takedawn-setup.exe Qt6Core.dll SDL2.dll upscaler/realesrgan-ncnn-vulkan.exe; do
    [ -f "$UP/$f" ] || die "$UP has no $f: not a Takedawn $UP_VERSION Windows folder"
done
for f in Takedawn-pal.exe Takedawn-ensv.exe; do
    LC_ALL=C grep -aq "Takedawn $UP_VERSION \"" "$UP/$f" || die "$UP/$f is not Takedawn $UP_VERSION"
done

# The main program: the North America disc's, with the four-player code.
"$REPO/tools/release/build-windows.sh" --no-setup --no-setup-window
B="$WORK/windows"
[ -f "$B/bin/Takedawn.exe" ] || die "no $B/bin/Takedawn.exe"

NAME="Takedawn-$VERSION-windows-x64"
PKG="$WORK/windows-4p/$NAME"
rm -rf "$WORK/windows-4p"; mkdir -p "$PKG"
# Upstream's folder as it is (not its uninstaller, nor anything else that
# is not part of the release), then this build's own files over it.
( cd "$UP" && find . -type f ! -name 'unins*' ! -path './data/*' ! -path './splitscreen-4p/*' \
      ! -name portable.txt -print0 ) | ( cd "$UP" && xargs -0 cp --parents -t "$PKG" )
cp "$B/bin/Takedawn.exe" "$PKG/Takedawn.exe"
# Ours and the toolkit's licences (the same set as upstream's, plus Qt's).
cp "$B/Takedawn/licenses/"* "$PKG/licenses/"
sed -e "s/@VERSION@/$VERSION/g" -e "s/@UPVERSION@/$UP_VERSION/g" \
    "$REPO/tools/release/windows/README-4p.txt" | tr -d '\r' | sed 's/$/\r/' > "$PKG/README.txt"
: > "$PKG/portable.txt"
sed 's/$/\r/' > "$PKG/licenses/SOURCE.txt" <<EOF
Takedawn $VERSION "$CODENAME" (four-player split screen)

Takedawn.exe (the North America disc's program) was built from:

  game     github.com/leanbgit/takedawn, tag v$VERSION
  toolkit  github.com/xenophobentx/xboxrecomp, tag takedawn-$UP_VERSION, with
           patches/xboxrecomp-4p.patch of the game repository applied
           (patches/apply-toolkit.sh). The toolkit holds the LGPL-2.1 files
           listed in xboxrecomp-NOTICE.txt: src/apu/apu_core.c, apu_dsp.c,
           apu_vp.c and their headers, from xemu.

Everything else in this folder (Takedawn-pal.exe, Takedawn-ensv.exe, the
setup window and its Qt files, SDL2.dll, the upscaler and the shaders) is
from the Takedawn $UP_VERSION release, unchanged:

  github.com/xenophobentx/takedawn/releases/tag/v$UP_VERSION
  (its own source: the tags v$UP_VERSION and takedawn-$UP_VERSION of
  xenophobentx/takedawn and xenophobentx/xboxrecomp)

That is the corresponding source the LGPL asks for; the build scripts are in
the game repository under tools/release/ (this zip: build-windows-4p.sh).

The game's own code and data are not part of this program's source: the
program is made from the user's disc's executable by the toolkit's
recompiler, and the data is installed from the user's own disc.
EOF

# What Takedawn.exe imports was checked by build-windows.sh; the folder must
# hold what it needs.
for f in SDL2.dll Takedawn.exe Takedawn-pal.exe Takedawn-ensv.exe takedawn-setup.exe README.txt portable.txt; do
    [ -f "$PKG/$f" ] || die "the folder lacks $f"
done

mkdir -p "$DIST"
ZIP="$DIST/$NAME-portable.zip"
rm -f "$ZIP"
( cd "$WORK/windows-4p" && find "$NAME" -exec touch -d "@${SOURCE_DATE_EPOCH:-$(date +%s)}" {} + \
      && find "$NAME" -type f | LC_ALL=C sort | zip -q -X -9 "$ZIP" -@ )
( cd "$DIST" && sha256sum "$(basename "$ZIP")" > SHA256SUMS )
say "$(basename "$ZIP"): $(( $(stat -c %s "$ZIP") / 1024 / 1024 )) MB, $(find "$PKG" -type f | wc -l) files"
say "sha256 $(cut -d' ' -f1 "$DIST/SHA256SUMS")"
