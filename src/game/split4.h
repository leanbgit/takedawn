/*
 * split4.h - four-player split screen (docs/split4.md).
 *
 * The title keeps its human players in fixed two-slot arrays. The lift moves
 * them (config/relocate.json, toolkit --relocate) into larger blocks right
 * after the image, which the runtime reserves for us (g_xbox_image_extra).
 * The addresses below must match config/relocate.json.
 */
#ifndef BO3_SPLIT4_H
#define BO3_SPLIT4_H

#include <stdint.h>

#define SPLIT4_MAX_PLAYERS   4u

/* The block after the image: 64 KB aligned after the USA image's end
 * (0x007792E0), so 0x00780000. split4_check() verifies it at boot. */
#define SPLIT4_EXTRA_BASE    0x00780000u
#define SPLIT4_EXTRA_BYTES   0x00040000u

/* The race world (a static object) and its human racers. */
#define SPLIT4_WORLD         0x0060EA00u
#define SPLIT4_HUMAN_OLD     0x0073A1D0u   /* world + 0x12B7D0, two slots */
#define SPLIT4_HUMAN_SIZE    0x000027E0u
#define SPLIT4_HUMAN         0x00780000u   /* relocated: four slots */

/* Before xbox_MemoryLayoutInit: ask for the block. */
void split4_reserve(void);
/* After it: the block must be where the lift put the arrays. Returns 0 and
 * prints why when it is not (the build then runs with corrupt memory). */
int split4_check(void);
/* BO3_SPLIT4=1: the two-player stage plays four (step 3, bring-up). */
int split4_enabled(void);
/* At start-up, before the guest runs (main.c): registers the mark hooks. */
void split4_start(void);
/* Wrappers of generated functions (recomp/recomp_manual.c). */
void split4_world_ctor(void (*gen)(void));    /* sub_001A9930 */
void split4_world_load(void (*gen)(void));    /* sub_001AA100 */
void split4_stage_start(void (*gen)(void));   /* sub_00017450 */
void split4_stage_setup(void (*gen)(void));   /* sub_000209A0 */
/* Once a flip (main.c): BO3_SPLIT4_LOG. */
void split4_log(uint32_t flip);

#endif
