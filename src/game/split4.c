/*
 * split4.c - four-player split screen (docs/split4.md).
 */
#include "split4.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

extern ptrdiff_t g_xbox_mem_offset;
#define S4_M8(va)  (*(const uint8_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_M16(va) (*(const int16_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_M32(va) (*(const uint32_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_MF(va)  (*(const float *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))

#include "xbox_memory_layout.h"

void split4_reserve(void)
{
    g_xbox_image_extra = SPLIT4_EXTRA_BYTES;
}

int split4_check(void)
{
    if (g_xbox_image_extra_base != SPLIT4_EXTRA_BASE) {
        fprintf(stderr, "[SPLIT4] the block after the image is at 0x%08X, the "
                "relocated arrays at 0x%08X: config/relocate.json does not fit "
                "this executable\n", g_xbox_image_extra_base, SPLIT4_EXTRA_BASE);
        return 0;
    }
    fprintf(stderr, "[SPLIT4] relocation block 0x%08X..0x%08X\n",
            SPLIT4_EXTRA_BASE, SPLIT4_EXTRA_BASE + SPLIT4_EXTRA_BYTES);
    return 1;
}

/* BO3_SPLIT4_LOG=<file>: every 30th flip, the race world's counts and each
 * human slot (index, phase, place, AI byte, position): what tells "all the
 * players are in the race and driving" in a headless run. Off unless set. */
static FILE *s_log;
static int s_log_tried;

void split4_log(uint32_t flip)
{
    const uint32_t w = SPLIT4_WORLD;
    unsigned k, nh;

    if (!s_log_tried) {
        const char *p = getenv("BO3_SPLIT4_LOG");
        s_log_tried = 1;
        if (p && *p) {
            s_log = fopen(p, "w");
            if (s_log)
                setvbuf(s_log, NULL, _IOLBF, 0);
        }
    }
    if (!s_log || !g_xbox_mem_offset || flip % 30u)
        return;
    nh = S4_M32(w + 0x12B7C0u);
    fprintf(s_log, "F %u racers %u humans %u ai %u", flip, S4_M32(w + 0x12B79Cu),
            nh, S4_M32(w + 0x12B7C4u));
    for (k = 0; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t h = SPLIT4_HUMAN + k * SPLIT4_HUMAN_SIZE;
        fprintf(s_log, " | h%u vt %08X idx %u ph %u pl %d ai %u pos %.1f,%.1f,%.1f",
                k, S4_M32(h), S4_M32(h + 0x27D0u), S4_M32(h + 0x134Cu),
                S4_M16(h + 0x10D0u), S4_M8(h + 0x27D8u),
                S4_MF(h + 0x40u), S4_MF(h + 0x44u), S4_MF(h + 0x48u));
    }
    fputc('\n', s_log);
}

/* ---- The four-player stage (step 3, bring-up) --------------------------
 *
 * BO3_SPLIT4=1 turns the title's two-player stage into a four-player one:
 * four human racers (and at most two AI rivals, six cars in all), setup
 * records and cameras for players 3 and 4. Without it nothing here acts and
 * the two-player stage is the title's. BO3_SPLIT4_AI=1 also has the title's
 * AI drive players 3 and 4 (a logic test without four pads). */

#include "platform/recomp_env.h"
#include "platform/recomp_patch.h"
#include "recomp_types.h"

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esi, g_esp;

#define S4_W8(va)  (*(uint8_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_W32(va) (*(uint32_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_WF(va)  (*(float *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))

#define S4_MODULE_CUR    0x004D5370u   /* the current module */
#define S4_VT_STAGE_2P   0x003A939Cu   /* the two-player stage module */
#define S4_STAGE_HUMANS  0xA8u         /* the module's racer counts (sub_00017450 */
#define S4_STAGE_AIS     0xACu         /*  passes them to sub_001AACB0) */
#define S4_HUMAN_VT      0x003B11A0u   /* CB3PlayerCar */
#define S4_SETUP         0x0078A000u   /* relocated setup records, 0x24 each */
#define S4_SETUP_SIZE    0x24u
#define S4_CAM           0x0078A100u   /* relocated player cameras, 0xC50 each */
#define S4_CAM_SIZE      0x0C50u
#define S4_CAR_AI        0x27D8u       /* byte: the AI drives this car */
#define S4_CAR_PHASE     0x134Cu       /* 2: in play */
#define S4_CAR_AI_RESET  0x19D0u
#define S4_CAR_DRIVER    0x1A00u

extern void sub_00015900(void);        /* the player camera's constructor (ecx) */
extern void sub_001705F0(void);        /* a racer's init (esi) */
extern void sub_001B4230(void);        /* the player camera's reset, vtable +0xC (ecx; mode, 0) */
extern void sub_00179760(void);        /* the AI driver's route reset (eax) */

static int s_on = -1, s_ai = -1;

int split4_enabled(void)
{
    if (s_on < 0) {
        const char *v = getenv("BO3_SPLIT4");
        s_on = v && *v && *v != '0';
        if (s_on)
            fprintf(stderr, "[SPLIT4] four-player stage on (BO3_SPLIT4)\n");
    }
    return s_on;
}

static int split4_ai34(void)
{
    if (s_ai < 0) {
        const char *v = getenv("BO3_SPLIT4_AI");
        s_ai = v && *v && *v != '0';
    }
    return s_ai;
}

static int stage_is_2p(void)
{
    uint32_t mod = S4_M32(S4_MODULE_CUR);
    return mod >= 0x10000u && mod < 0x04000000u && S4_M32(mod) == S4_VT_STAGE_2P;
}

/* The world's constructor (sub_001A9930) sets the human vtables of slots 0
 * and 1 one by one; slots 2 and 3 get theirs here (always: every slot is
 * built, config/relocate.json "immediates", the stage decides how many race). */
void split4_world_ctor(void (*gen)(void))
{
    unsigned k;
    gen();
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++)
        S4_W32(SPLIT4_HUMAN + k * SPLIT4_HUMAN_SIZE) = S4_HUMAN_VT;
}

/* The world's load steps (sub_001AA100): step 1 resets the cameras of
 * players 1 and 2 (vtable +0xC with 0xE, 0); those of 3 and 4 the same. */
void split4_world_load(void (*gen)(void))
{
    uint32_t step = S4_M32(SPLIT4_WORLD + 0x144384u);
    unsigned k;
    gen();
    if (!split4_enabled() || step != 1)
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t cam = S4_CAM + k * S4_CAM_SIZE;
        uint32_t a = g_eax, c = g_ecx, d = g_edx, sp = g_esp;
        if (!S4_M32(cam))
            continue;
        g_ecx = cam;
        PUSH32(g_esp, 0);
        PUSH32(g_esp, 0xE);
        PUSH32(g_esp, 0);
        sub_001B4230();
        g_eax = a; g_ecx = c; g_edx = d; g_esp = sp;
    }
}

/* The stage's start (sub_00017450, ecx the module): four humans, the AIs to
 * fill six cars, and cameras 2 and 3 built once (the app's constructor
 * builds two, 0x157D3). */
void split4_stage_start(void (*gen)(void))
{
    uint32_t mod = g_ecx;
    unsigned k;

    if (split4_enabled() && mod && S4_M32(mod) == S4_VT_STAGE_2P) {
        uint32_t ais = S4_M32(mod + S4_STAGE_AIS);
        S4_W32(mod + S4_STAGE_HUMANS) = SPLIT4_MAX_PLAYERS;
        if (ais > 6u - SPLIT4_MAX_PLAYERS)
            S4_W32(mod + S4_STAGE_AIS) = 6u - SPLIT4_MAX_PLAYERS;
        for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
            uint32_t cam = S4_CAM + k * S4_CAM_SIZE;
            if (!S4_M32(cam)) {
                uint32_t a = g_eax, c = g_ecx, d = g_edx, sp = g_esp;
                g_ecx = cam;
                PUSH32(g_esp, 0);
                sub_00015900();
                g_eax = a; g_ecx = c; g_edx = d; g_esp = sp;
            }
        }
        fprintf(stderr, "[SPLIT4] stage start: %u humans, %u AIs\n",
                S4_M32(mod + S4_STAGE_HUMANS), S4_M32(mod + S4_STAGE_AIS));
    }
    g_ecx = mod;
    gen();
}

/* The two-player stage's set-up (sub_000209A0) fills the setup records of
 * players 1 and 2 (the profile of the pad's port, the flags); players 3 and
 * 4 take pads 3 and 4 with player 1's profile and flags. */
void split4_stage_setup(void (*gen)(void))
{
    unsigned k;
    gen();
    if (!split4_enabled() || !stage_is_2p())
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t r = S4_SETUP + k * S4_SETUP_SIZE;
        S4_W32(r + 0x00) = S4_M32(S4_SETUP + 0x00);
        S4_W32(r + 0x1C) = S4_M32(S4_SETUP + 0x1C);
        S4_W8(r + 0x20) = S4_M8(S4_SETUP + 0x20);
        S4_W8(r + 0x18) = (uint8_t)k;          /* the pad's port */
    }
}

/* The car update mark (ap_update, sub_0018C510, esi the car): with
 * BO3_SPLIT4_AI the title's AI takes players 3 and 4 once they are in play,
 * as autopilot.c does for player 1. */
static void split4_car_update(void)
{
    uint32_t car = g_esi;

    if (car != SPLIT4_HUMAN + 2 * SPLIT4_HUMAN_SIZE &&
        car != SPLIT4_HUMAN + 3 * SPLIT4_HUMAN_SIZE)
        return;
    if (S4_M32(car + S4_CAR_PHASE) != 2 || S4_M8(car + S4_CAR_AI))
        return;
    {
        uint32_t a = g_eax, c = g_ecx, d = g_edx, sp = g_esp;
        S4_W8(car + S4_CAR_AI) = 1;
        S4_W32(car + S4_CAR_AI_RESET + 0x0) = 0;
        S4_W32(car + S4_CAR_AI_RESET + 0x4) = 0;
        S4_W32(car + S4_CAR_AI_RESET + 0x8) = 0;
        S4_W32(car + S4_CAR_AI_RESET + 0xC) = 0;
        g_eax = car + S4_CAR_DRIVER;
        PUSH32(g_esp, 0);
        sub_00179760();
        g_eax = a; g_ecx = c; g_edx = d; g_esp = sp;
    }
    fprintf(stderr, "[SPLIT4] the AI drives player %u\n",
            (car - SPLIT4_HUMAN) / SPLIT4_HUMAN_SIZE + 1);
}

void split4_start(void)
{
    if (split4_enabled() && split4_ai34() && !recomp_env_on(RENV_AUTOPILOT))
        recomp_patch_on_mark("ap_update", split4_car_update);
}

/* The guest call chain off the guest stack (return addresses the lifted code
 * pushed; values that only look like code are printed too). For diagnosis:
 * the wild function pointer report calls it once per address. */
void split4_guest_chain(void)
{
    uint32_t k, shown = 0;
    if (!g_xbox_mem_offset)
        return;
    for (k = 0; k < 128u && shown < 8u; k++) {
        uint32_t v = S4_M32(g_esp + k * 4u);
        if (v >= g_xbox_code_lo && v < g_xbox_code_hi) {
            fprintf(stderr, "         guest [esp+%u] %08X\n", k * 4u, v);
            shown++;
        }
    }
}
