# Four-player split screen (work in progress)

Goal: the title's two-player split screen with four players, one view in
each quarter of the screen, each player with their own pad and car, exactly
as the two-player stage behaves today.

Status: step 1 of the plan below. The human racers are relocated and two
players play as before; there is no four-player stage yet.

## How the title holds its players

All addresses NTSC-U.

The race world is one static object at `0x60EA00`:

| World offset | Address | What |
|---|---|---|
| `+0x12B79C` | `0x73A19C` | racers in the race (at most 6) |
| `+0x12B7A8` | `0x73A1A8` | the six racer pointers |
| `+0x12B7C0` | `0x73A1C0` | human racers (the "player count", 123 reads) |
| `+0x12B7C4` | `0x73A1C4` | AI racers |
| `+0x12B7D0` | `0x73A1D0` | human racers, **2 x 0x27E0** (vtable `0x3B11A0`) |
| `+0x130790` | `0x73F190` | AI racers, 5 x 0x2470 (vtable `0x3B1204`) |
| `+0x13BDC0` | `0x74A7C0` | network racers, 0x1AB0 each |

A human racer is an AI racer (`0x2470` bytes) and more; `+0x2470` points to
its setup record (`0x4CFAD0 + 0x24 x player`: the pad port at `+0x18`, the
profile), `+0x27D0` is its player index, `+0x27D8` the "AI drives" byte,
`+0x134C` its phase (1 before the GO, 2 in play, 3 finished).

`sub_001AACB0(world, humans, ais, net)` fills the racer table from the
stage's counts (the two-player stage module, vtable `0x3A939C`, passes 2).
The world's constructor `sub_001A9930` sets the human vtables for slots 0
and 1 one by one.

Two slots are all there is room for: the AI racers start right after the
second human. Other per-player data is laid out the same way (two slots
packed against the next global): the race records (`0x4D0738`, `0xC50`), the
setup records, the views (`0x4D6200`, `0xA0`) and cameras (`0x4D6340`), the
layout rectangles (`0x54F900 + 0x48 / + 0xB8`).

## The approach

**Relocation at lift time.** `config/relocate.json` moves each two-slot
array into a block after the image (toolkit `tools/recomp/relocate.py`,
`--relocate`; the runtime reserves the block, `g_xbox_image_extra`, and
`src/game/split4.c` checks it is at `0x780000`). Every immediate and
displacement inside the old array becomes the same offset in the new one,
including world-relative ones (`[esi + 0x12B7D0]`), and a loop's end
sentinel (`cmp esi, 0x73F190`) becomes the new end. The port's own C follows
through `BO3_VA` (`region.h`, `BO3_SPLIT4_RELOC`). The lift writes every
rewrite to `build/xr/recomp/relocate_sites.txt`.

**The sites that name slot 1.** A rewrite whose old address is in the second
slot is code written out for two players (the world constructor, the stage's
render, results). Each needs its third and fourth counterparts: a guest
patch or a function override in `recomp_manual.c`. The humans relocation has
27 such sites.

## Plan

1. Relocate the human racers; two players unchanged. **(done)**
2. Relocate the setup records, race records, views and cameras the same way;
   two players unchanged.
3. A four-player stage: count 4, the slot 2 and 3 counterparts of every slot
   1 site, four views in quarters (`sub_00053700`'s layout, the stage render
   `sub_00020B30`).
4. Pads 3 and 4 driving (controls.c's gate, the setup records' ports).
5. HUD per view, results, sound.
6. The front end: players 3 and 4 choose their cars (first: a setting).

## Checks

Headless, no reference saves needed (`SAVES=empty`: a new profile that is
not saved), the null backend for logic, Vulkan for pictures:

    SAVES=empty RECOMP_PB_BACKEND=null STOP_AT_END=1 \
        tools/headless/split_scene.sh runs/s2 race \
        BO3_SPLIT4_LOG=$PWD/runs/s2/split4.txt

`BO3_SPLIT4_LOG` writes, every 30 flips, the racer counts and each human
slot (vtable, index, phase, place, AI byte, position).

`RECOMP_WATCH=0x73A1C0` names the writer of the player count (on Linux the
toolkit now scans the guest stack for the chain).
