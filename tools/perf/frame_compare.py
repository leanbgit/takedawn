#!/usr/bin/env python3
"""frame_compare.py - the frame check's two runs, shot by shot.

    tools/perf/frame_compare.py RUN_A RUN_B [--limits same_backend]

Each run is a frame_check.sh folder: shots.txt ("<flip> <mark>") and the
frames fb_flip_NNNNN.bmp. Shots are paired by their mark, so the two runs may
reach a mark at different flips. Each pair is EXACT (same pixels), CLOSE
(within the limits of tools/headless/golden.json, same_backend by default:
the ones the golden frames use for determinism) or FAIL; exit 1 on a FAIL or
a missing shot. The image reading and the statistics are golden.py's.
The EA Trax song box is masked in every shot and the main menu's shot is
left out (MASKS, SKIP: both differ between runs of one build).
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'headless'))
import golden  # noqa: E402


# Parts of a picture that differ between runs for reasons outside the
# toolkit (golden.json's own reasons): masked in every shot, or the shot
# left out.
MASKS = [{"rect": [20, 384, 345, 70],
          "why": "EA Trax box: the song is picked at random (golden.json, menu_main)"}]
SKIP = {"main menu": "its background is the attract movie, on its own clock",
        "bench end": "the run exits there"}


def shots(d):
    out = {}
    with open(os.path.join(d, 'shots.txt')) as f:
        for line in f:
            if line.strip():
                flip, name = line.rstrip('\n').split(' ', 1)
                out[name] = os.path.join(d, 'fb_flip_%05u.bmp' % int(flip))
    return out


def main(argv):
    args = [a for a in argv if not a.startswith('--')]
    kind = 'same_backend'
    for a in argv:
        if a.startswith('--limits='):
            kind = a.split('=', 1)[1]
    if len(args) != 2:
        sys.exit(__doc__)
    with open(golden.MANIFEST) as f:
        lim = json.load(f)['limits'][kind]
    a, b = shots(args[0]), shots(args[1])
    bad = 0
    for name in a:
        if name in SKIP:
            print('%-14s SKIP  %s' % (name, SKIP[name]))
            continue
        if name not in b or not os.path.exists(a[name]) or not os.path.exists(b[name]):
            print('%-14s MISSING' % name)
            bad += 1
            continue
        v, note = golden.compare(golden.read_image(b[name]), golden.read_image(a[name]), lim, MASKS)
        print('%-14s %-5s %s' % (name, v, note))
        bad += v == 'FAIL'
    for name in b:
        if name not in a and name not in SKIP:
            print('%-14s MISSING in the first run' % name)
            bad += 1
    print('%d shots, %d failing' % (len(a), bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
