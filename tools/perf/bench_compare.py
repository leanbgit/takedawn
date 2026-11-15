#!/usr/bin/env python3
"""bench_compare.py - one table from a BO3_BENCH run, or two side by side.

    tools/perf/bench_compare.py bench/4p-a                 # one run
    tools/perf/bench_compare.py bench/4p-a bench/4p-b      # compared (b - a)

A run's folder (run-bench.cmd, src/game/bench.c) has marks.txt (the
executor's flip at each mark), ft.csv (RECOMP_TRACE=frametime), vk.csv
(vk_frame) and pb.csv (pb_split). Only the flips between the marks
"bench start" and "bench end" count.

pb.csv's per-draw columns are scaled to the whole picture by the toolkit
(since the roadmap's step 2); a file from before that has the timed share
only, about a quarter (`--old-pb` multiplies by draws / draws_timed).
"""
import argparse
import csv
import math
import os
import sys


def marks(d):
    m = {}
    with open(os.path.join(d, 'marks.txt')) as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            flip, ms, what = line.rstrip('\n').split(' ', 2)
            m[what] = (int(flip), float(ms))
    return m


def rows(path, lo, hi):
    out = []
    if not os.path.exists(path):
        return out
    with open(path, newline='') as f:
        for r in csv.DictReader(f):
            try:
                fl = int(r.get('flip') or 0)
            except ValueError:
                continue
            if lo <= fl < hi:
                out.append(r)
    return out


def num(r, k):
    try:
        return float(r.get(k) or 0)
    except ValueError:
        return 0.0


def mean(rs, k):
    return sum(num(r, k) for r in rs) / len(rs) if rs else float('nan')


def pct(vals, p):
    if not vals:
        return float('nan')
    v = sorted(vals)
    return v[min(len(v) - 1, int(math.ceil(p / 100.0 * len(v))) - 1)]


VK_PARTS = [('gather', 'vertex gather'), ('ubo', 'constants'), ('tex', 'texture binds'),
            ('cmd', 'vkCmd calls'), ('pso', 'pipeline lookup'), ('prim', 'prim to list'),
            ('stages', 'stages'), ('fs', 'fs select'), ('tset', 'texture set'),
            ('surface', 'surface'), ('enc', 'encoder'), ('vs', 'vs lookup'),
            ('vpc', 'vp consts'), ('hdrz', 'hdr/zbias')]
CLS = [('imm', 'immediate vertices'), ('comb', 'combiners'), ('xconst', 'transform consts'),
       ('index', 'index data'), ('inline', 'inline arrays'), ('tex', 'texture state'),
       ('state', 'other state'), ('xprog', 'vertex programs'), ('vfmt', 'vertex formats'),
       ('surf', 'surfaces'), ('sync', 'sync'), ('clear', 'clears')]


def summary(d, old_pb):
    m = marks(d)
    if 'bench start' not in m or 'bench end' not in m:
        sys.exit('%s: no "bench start" / "bench end" in marks.txt (the run did not finish)' % d)
    lo, hi = m['bench start'][0], m['bench end'][0]
    wall = (m['bench end'][1] - m['bench start'][1]) / 1000.0
    ft = rows(os.path.join(d, 'ft.csv'), lo, hi)
    vk = rows(os.path.join(d, 'vk.csv'), lo, hi)
    pb = rows(os.path.join(d, 'pb.csv'), lo, hi)
    s = {}
    s['flips'] = hi - lo
    s['seconds'] = wall
    s['fps'] = (hi - lo) / wall if wall > 0 else float('nan')
    fms = [num(r, 'frame_ms') for r in ft]
    s['frame ms p50'] = pct(fms, 50)
    s['frame ms p95'] = pct(fms, 95)
    s['frame ms p99'] = pct(fms, 99)
    s['over 16.7 ms %'] = 100.0 * sum(1 for v in fms if v > 16.7) / len(fms) if fms else float('nan')
    for k, name in (('ex_busy', 'executor busy ms'), ('ex_draw', '  of it draws ms'),
                    ('ex_idle', 'executor idle ms'), ('m_game', 'game code ms'),
                    ('m_d3d', 'game D3D ms'), ('m_host', 'game host/waits ms'),
                    ('gpu_ms', 'GPU ms')):
        s[name] = mean(ft, k)
    s['draws'] = mean(vk, 'draws')
    s['passes'] = mean(vk, 'passes')
    s['draws main'] = mean(vk, 'd_main')
    s['draws square'] = mean(vk, 'd_square')
    if pb:
        draws = mean(pb, 'draws')
        k = 1.0
        if old_pb:
            dt = mean(pb, 'draws_timed')
            k = draws / dt if dt else 0.0
        tot = 0.0
        for key, name in VK_PARTS:
            v = mean(pb, 'vk_%s_us' % key) * k
            tot += v
            s['draw: %s us' % name] = v / draws if draws else float('nan')
        s['draw: total us'] = tot / draws if draws else float('nan')
        for key, name in CLS:
            s['walk: %s ms' % name] = mean(pb, 'us_%s' % key) * (k if old_pb else 1.0) / 1000.0
    return s


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('runs', nargs='+')
    ap.add_argument('--old-pb', action='store_true')
    a = ap.parse_args()
    sums = [summary(d, a.old_pb) for d in a.runs[:2]]
    names = [os.path.basename(os.path.normpath(d)) for d in a.runs[:2]]
    w = max(len(k) for k in sums[0])
    head = '%-*s %12s' % (w, '', names[0][-12:])
    if len(sums) == 2:
        head += ' %12s %9s' % (names[1][-12:], 'change')
    print(head)
    for k in sums[0]:
        v0 = sums[0][k]
        line = '%-*s %12.2f' % (w, k, v0)
        if len(sums) == 2:
            v1 = sums[1].get(k, float('nan'))
            ch = (v1 - v0) / v0 * 100.0 if v0 else float('nan')
            line += ' %12.2f %8.1f%%' % (v1, ch)
        print(line)


if __name__ == '__main__':
    main()
