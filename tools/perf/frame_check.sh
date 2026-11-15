#!/usr/bin/env bash
# frame_check.sh <outdir> <1p|2p|4p> [KEY=VALUE...]
#
# The frame check of the toolkit roadmap (step 3): the benchmark race
# (BO3_BENCH, src/game/bench.c), headless, with BO3_LOCKSTEP=1 (the same race
# in every run) and a frame dumped three flips after every mark (shots.txt
# lists them). Compare two runs with frame_compare.py:
#
#   tools/perf/frame_check.sh runs/fc/ref-4p 4p
#   tools/perf/frame_check.sh runs/fc/new-4p 4p BIN=/path/to/other/burnout3
#   tools/perf/frame_compare.py runs/fc/ref-4p runs/fc/new-4p
#
# FLIPS (default 600) is the measured part; extra KEY=VALUE pairs go to the
# game's environment (BIN= picks another executable). Run from the repo.
set -eu
OUT=$1; MODE=$2; shift 2
BIN=./bin/burnout3
EXTRA=()
for kv in "$@"; do
    case "$kv" in BIN=*) BIN=${kv#BIN=} ;; *) EXTRA+=("$kv") ;; esac
done
rm -rf "$OUT"
mkdir -p "$OUT/saves"
: > "$OUT/pad.txt"
: > "$OUT/dumpat.txt"
OUT=$(cd "$OUT" && pwd)
SPLIT=()
if [ "$MODE" = 4p ]; then
    SPLIT=(BO3_SPLIT4=4 BO3_SPLIT4_AI=4 BO3_SPLIT4_CARS=copy)
fi
env BO3_BENCH="$MODE" BO3_BENCH_OUT="$OUT" BO3_BENCH_FLIPS="${FLIPS:-600}" \
    BO3_BENCH_SHOTS=1 BO3_LOCKSTEP=1 BO3_UNLOCK_ALL=1 \
    RECOMP_PAD_LIVE="$OUT/pad.txt" RECOMP_SAVE_DIR="$OUT/saves" RECOMP_SETTINGS=none \
    RECOMP_HEADLESS=1 RECOMP_PB_BACKEND=vulkan RECOMP_USB_PADS=4 \
    RECOMP_FB_DUMP="$OUT/fb_" RECOMP_FB_DUMP_AT="@$OUT/dumpat.txt" \
    RECOMP_STDIO_LOG="$OUT/log.txt" \
    "${SPLIT[@]}" "${EXTRA[@]}" "$BIN" || true
tail -3 "$OUT/marks.txt"
