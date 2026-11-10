#!/usr/bin/env bash
# Apply the four-player split screen's toolkit changes (docs/split4.md) to
# the xboxrecomp checkout beside this one (XBOXRECOMP_DIR, default
# ../xboxrecomp), on its tag takedawn-0.3.1. Idempotent: does nothing when
# the patch is already in.
set -eu
BO3="$(cd "$(dirname "$0")/.." && pwd)"
TK="${XBOXRECOMP_DIR:-$BO3/../xboxrecomp}"
P="$BO3/patches/xboxrecomp-4p.patch"
if git -C "$TK" apply --reverse --check "$P" 2>/dev/null; then
    echo "toolkit: four-player patch already applied"
elif git -C "$TK" apply --check "$P"; then
    git -C "$TK" apply "$P"
    echo "toolkit: four-player patch applied to $TK"
else
    echo "toolkit: the patch does not apply; check out takedawn-0.3.1 in $TK" >&2
    exit 1
fi
