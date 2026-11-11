# Four-player split screen (work in progress)

Goal: the title's two-player split screen with four players, one view in
each quarter of the screen, each player with their own pad and car, exactly
as the two-player stage behaves today.

Status: step 3 in progress. With `BO3_SPLIT4=1` the two-player stage
races four humans and two AI rivals to the end without a fault (headless,
null backend, players 3 and 4 driven by the title's AI with
`BO3_SPLIT4_AI=1`). Only two views are drawn yet. Without `BO3_SPLIT4` two
players race as before.

## Building this branch

The branch needs two toolkit changes (the `--relocate` lift option and the
runtime's block after the image). They are kept here, in
`patches/xboxrecomp-4p.patch`; apply them to the toolkit checkout
(`xenophobentx/xboxrecomp`, tag `takedawn-0.3.1`) before building:

    git clone -b takedawn https://github.com/xenophobentx/xboxrecomp ../xboxrecomp
    git -C ../xboxrecomp checkout takedawn-0.3.1
    patches/apply-toolkit.sh
    XBOXRECOMP_DIR=../xboxrecomp ./regen.sh --disasm

then build as in [building.md](building.md).

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

The cameras: the app object (`0x4A71A0`) holds the players' cameras at
`+0x28980` (`0x4CFB20`, `0xC50` each, indexed by the human's `+0x27D0`), built
two by the app constructor (`0x157D3: mov ebx, 2`). A third camera object
follows (`0x4D13C0`); it is reached only by fixed addresses, so it stays put
and the two player cameras move to a four-slot block.

## Plan

1. Relocate the human racers; two players unchanged. **(done)**
2. Relocate the setup records and the player cameras; two players
   unchanged. **(done)** Still to do the same way: the views (`0x4D6200`) and
   RenderWare cameras (`0x4D6340`), the layout rectangles.
3. A four-player stage: count 4 (`split4_stage_start`), the slot 2 and 3
   counterparts of every slot 1 site, four views in quarters
   (`sub_00053700`'s layout, the split view builder `sub_0002F480`, the stage
   render `sub_00020B30` / `sub_001AE2F0`). **The race runs (done); the views
   are next.** Done so far:
   - relocated as well: per-player camera data (`0x737710`, `0x1050`),
     per-view data (`0x7397C8`, `0x1CC`), the per-player car pointer table
     (`0x739C58`, 4 bytes), the human cars' physics objects (`0x667E90`,
     `0x4AD0`, object-relative at listed sites only);
   - counts raised to four at lift time (`"immediates"`): the cameras built
     by the app (`0x157D3`), the physics objects built and set up
     (`0x1A99F8`, `0x11028E`), the world's human set-up (`0x1A9EE8`);
   - `recomp_manual.c` wrappers: the world's constructor (human vtables),
     the world's load step 1 (camera reset for 3 and 4), the stage's start
     (four humans, at most two AIs, cameras built), the two-player set-up
     (setup records for pads 3 and 4).
   - four views (`split4.c`, the four-view part): views 3 and 4 built like
     the title's two (`sub_0002ECC0`), the split builder makes four quarter
     sub-rasters (`sub_0002F480`), the view select takes modes 3 and 4
     (`sub_0002F330`), the stage render draws views 2 and 3 after the
     title's two (`sub_001AE2F0`);
   - cameras 2 and 3: their player index (`sub_001673E0`, which the app's
     init runs for 0 and 1 only; without it they bind to view 0), their own
     view at the bind (`sub_001674B0`: link and the sub-object's vtable + 8
     set-up), and camera 1's global mode switches (intro, start, finish,
     replays: eleven unrolled call sites of `sub_001B4230`) repeated for
     them, as are the app's per-frame camera update (`sub_000170B0` names
     cameras 0 and 1: mark `s4_cam_frame`) and its camera reset
     (`sub_00016D00`: mark `s4_cam_reset`);
   - track streaming: the world's load starts stream window 1 (around
     player 2) only for exactly two humans (`sub_001AA100` steps 0x10 and
     0x11, `cmp [count], esi`; marks `s4_stream_*` in `config/patches.json`)
     and waits for it likewise (`sub_001AA920`). Without window 1, every car
     that looks its ground up there (player 2's; 3's and 4's, which the
     title gives view 1) fell through the track. The streamer's focus loop
     end (`immediates` 0x1A2265) is the end of slot 1 (0x784FC0);
   - views 3 and 4 draw the track of the window their car is in (a copy of
     window 0 or 1, by the car's view byte);
   - HUD: two panels (the manager has two records, its own fields follow),
     placed in the top quarters (`sub_00053700`); the state calls for
     players 3 and 4 (`sub_00053D20`) skipped, as they would write into the
     layout data after the two records.
4. Pads 3 and 4 driving (controls.c's gate, the setup records' ports).
5. HUD panels for players 3 and 4, results, sound.
6. The front end: players 3 and 4 choose their cars (first: a setting).

## Known limitations

- **Track streaming follows two cars.** The streamer (`sub_001A20F0`) keeps
  two foci in fixed locals (two section words, a count of 2, two pointers)
  and two windows (world + 0x12ADC8, 0x1CC each): it follows players 1 and 2
  only (`immediates`: its loop ends after slot 1). Players 3 and 4 find
  their ground in either window (the title's fallback switches a car's view
  byte between 0 and 1); one far from both drives into track that is not
  loaded and falls until the title puts it back. Four foci need that
  function rewritten.
- **No HUD for players 3 and 4.** The HUD manager (0x54F900) holds two
  player records with its own fields right after them.

## Checks

Headless, no reference saves needed (`SAVES=empty`: a new profile that is
not saved), the null backend for logic, Vulkan for pictures:

    SAVES=empty RECOMP_PB_BACKEND=null STOP_AT_END=1 \
        tools/headless/split_scene.sh runs/s2 race \
        BO3_SPLIT4_LOG=$PWD/runs/s2/split4.txt

`BO3_SPLIT4_LOG` writes, every 30 flips, the racer counts and each human
slot (vtable, index, phase, place, AI byte, position), each car's track
section and view byte (`sec`: -1 is no ground), the streamer's flags and
each stream window's first section (`view`: -1 is not streaming).

Four players (logic, the AI driving 3 and 4):

    SAVES=empty RECOMP_PB_BACKEND=null STOP_AT_END=1 DRIVE_SECS=60 \
        tools/headless/split_scene.sh runs/s4 race BO3_SPLIT4=1 \
        BO3_SPLIT4_AI=1 BO3_SPLIT4_LOG=$PWD/runs/s4/split4.txt

`RECOMP_WATCH=0x73A1C0` names the writer of the player count (on Linux the
toolkit now scans the guest stack for the chain).

## Sites that name slot 1 (or a loop's end)

From `relocate_sites.txt` (steps 1 and 2). Each is code written for two
players and needs the third and fourth player's counterpart in step 3.
Also known, not in this list: `0x157D3` (`mov ebx, 2`, two cameras built),
`0x167F0E` (`index != 1`: "the other player"), `0x00020AAA`-area set-up of
the stage (`sub_000209A0`), the render `sub_00020B30`.

| Site | Array | What |
|---|---|---|
| `0x000127E9` | setup | slot 1 `+0x20` (mem) |
| `0x000148B4` | setup | slot 1 `+0x20` (mem) |
| `0x00015EAA` | cameras | slot 1 `+0x0` (rel) |
| `0x00016CBC` | cameras | slot 1 `+0xC46` (rel) |
| `0x00016D9F` | cameras | slot 1 `+0x0` (rel) |
| `0x00016DA5` | cameras | slot 1 `+0x0` (rel) |
| `0x00017186` | cameras | slot 1 `+0x0` (rel) |
| `0x00018997` | cameras | slot 1 `+0xC3C` (mem) |
| `0x0001A078` | setup | slot 1 `+0x20` (mem) |
| `0x0001A86C` | setup | slot 1 `+0x20` (mem) |
| `0x0001A887` | setup | slot 1 `+0x1C` (mem) |
| `0x0001AB50` | setup | slot 1 `+0x20` (mem) |
| `0x0001AE9E` | setup | slot 1 `+0x20` (mem) |
| `0x0001AEA4` | setup | slot 1 `+0x1C` (mem) |
| `0x00020822` | cameras | slot 1 `+0xC3C` (mem) |
| `0x000209F6` | setup | slot 1 `+0x18` (mem) |
| `0x00020A23` | setup | slot 1 `+0x0` (mem) |
| `0x00020A3C` | setup | slot 1 `+0x20` (mem) |
| `0x00020A43` | setup | slot 1 `+0x1C` (mem) |
| `0x00020B9A` | humans | slot 1 `+0x0` (imm) |
| `0x00020CB0` | setup | slot 1 `+0x20` (mem) |
| `0x00020CB7` | setup | slot 1 `+0x1C` (mem) |
| `0x00024461` | humans | slot 1 `+0x1138` (mem) |
| `0x0002446C` | humans | slot 1 `+0x0` (imm) |
| `0x0002447C` | humans | slot 1 `+0x16C8` (mem) |
| `0x0002479A` | cameras | slot 1 `+0x0` (mem) |
| `0x000247A4` | cameras | slot 1 `+0x0` (imm) |
| `0x0002488B` | cameras | slot 1 `+0x0` (mem) |
| `0x00024895` | cameras | slot 1 `+0x0` (imm) |
| `0x000248A4` | cameras | slot 1 `+0x0` (mem) |
| `0x000248AE` | cameras | slot 1 `+0x0` (imm) |
| `0x00024914` | cameras | slot 1 `+0x0` (mem) |
| `0x0002491E` | cameras | slot 1 `+0x0` (imm) |
| `0x00026003` | cameras | slot 1 `+0x0` (mem) |
| `0x0002600C` | cameras | slot 1 `+0x0` (imm) |
| `0x000261B8` | cameras | slot 1 `+0x0` (mem) |
| `0x000261C1` | cameras | slot 1 `+0x0` (imm) |
| `0x00026958` | cameras | slot 1 `+0x0` (mem) |
| `0x00026960` | cameras | slot 1 `+0x0` (imm) |
| `0x00026A4C` | cameras | slot 1 `+0x0` (mem) |
| `0x00026A56` | cameras | slot 1 `+0x0` (imm) |
| `0x00026CEA` | cameras | slot 1 `+0x0` (mem) |
| `0x00026CF4` | cameras | slot 1 `+0x0` (imm) |
| `0x00027232` | cameras | slot 1 `+0x0` (mem) |
| `0x00027237` | cameras | slot 1 `+0x0` (imm) |
| `0x0002A10A` | cameras | slot 1 `+0xC47` (mem) |
| `0x0002D946` | humans | slot 1 `+0x1768` (mem) |
| `0x0002D969` | humans | slot 1 `+0x1758` (mem) |
| `0x000508E2` | humans | slot 1 `+0x0` (imm) |
| `0x000535B5` | humans | slot 1 `+0x0` (imm) |
| `0x000535BF` | humans | slot 1 `+0x16D0` (imm) |
| `0x00060FDB` | humans | slot 1 `+0x16D0` (imm) |
| `0x00060FE7` | humans | slot 1 `+0x1768` (mem) |
| `0x00061A3C` | humans | slot 1 `+0x16D0` (mem) |
| `0x00061ACD` | humans | slot 1 `+0x16D8` (mem) |
| `0x00067C49` | setup | slot 1 `+0x0` (imm) |
| `0x00072D43` | humans | slot 1 `+0x1768` (mem) |
| `0x00072F38` | cameras | slot 1 `+0x0` (mem) |
| `0x00072F40` | cameras | slot 1 `+0x0` (imm) |
| `0x00073536` | humans | slot 1 `+0x1764` (mem) |
| `0x00073836` | humans | slot 1 `+0x1764` (mem) |
| `0x00073A92` | humans | slot 1 `+0x1768` (mem) |
| `0x0007A5E2` | setup | slot 1 `+0x4` (mem) |
| `0x0007A5EE` | setup | slot 1 `+0x4` (imm) |
| `0x0007A9AF` | setup | slot 1 `+0x0` (imm) |
| `0x000B6538` | humans | slot 1 `+0x1768` (mem) |
| `0x000B6582` | humans | slot 1 `+0x1768` (mem) |
| `0x000B67FA` | humans | slot 1 `+0x1984` (imm) |
| `0x000B6890` | humans | slot 1 `+0x1768` (mem) |
| `0x000E5138` | humans | slot 1 `+0x1768` (mem) |
| `0x000E608D` | humans | slot 1 `+0x1768` (mem) |
| `0x000E7A5A` | humans | slot 1 `+0x1984` (imm) |
| `0x00167DD6` | cameras | slot 1 `+0xC20` (mem) |
| `0x00167DE4` | cameras | slot 1 `+0xC18` (mem) |
| `0x00167DF9` | cameras | slot 1 `+0xC47` (mem) |
| `0x0017E28A` | humans | slot 1 `+0x1754` (mem) |
| `0x001A2265` | humans | loop end `0x0073F190` -> `0x00789F80` |
| `0x001A9950` | humans | slot 1 `+0x0` (rel) |
| `0x001AA142` | cameras | slot 1 `+0x0` (mem) |
| `0x001AA14C` | cameras | slot 1 `+0x0` (imm) |
| `0x001AA290` | humans | slot 1 `+0x27D9` (rel) |
