/*
 * split4.c - four-player split screen (docs/split4.md).
 */
#include "split4.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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
    /* Each car's track section and view (physics +0x216, +0x217) and each
     * view's streamed-section window (world + 0x12ADC8, relocated: its first
     * word, -1 when nothing streams for it). */
    fprintf(s_log, " | sec");
    for (k = 0; k < SPLIT4_MAX_PLAYERS; k++)
        fprintf(s_log, " %d/%d", (int8_t)S4_M8(0x007A0000u + k * 0x4AD0u + 0x216u),
                (int8_t)S4_M8(0x007A0000u + k * 0x4AD0u + 0x217u));
    /* The track streamer (world + 0x4DE0): +0x363B9 two foci (players 1
     * and 2), +0x363BB on, +0x363BD. */
    fprintf(s_log, " | strm %u %u %u", S4_M8(0x006137E0u + 0x363B9u),
            S4_M8(0x006137E0u + 0x363BBu), S4_M8(0x006137E0u + 0x363BDu));
    /* Traffic: the streamer's traffic cars (+ 0x127D0, 0x180 each, + 0x363AB
     * of them) in use (a bit each in the words at + 0x362E0), and its flags
     * + 0x363B7, + 0x363B8. */
    {
        uint32_t st = 0x006137E0u, n = S4_M8(st + 0x363ABu), used = 0, i;
        for (i = 0; i < n; i++)
            used += (S4_M32(st + 0x362E0u + (i / 32u) * 4u) >> (i % 32u)) & 1u;
        fprintf(s_log, " | traffic %u/%u %u %u", used, n, S4_M8(st + 0x363B7u), S4_M8(st + 0x363B8u));
        /* and how many of them are within 250 of each player (+ 0 the
         * car's position) */
        fprintf(s_log, " near");
        for (k = 0; k < SPLIT4_MAX_PLAYERS; k++) {
            uint32_t h = SPLIT4_HUMAN + k * SPLIT4_HUMAN_SIZE, cnt = 0;
            for (i = 0; i < n; i++)
                if ((S4_M32(st + 0x362E0u + (i / 32u) * 4u) >> (i % 32u)) & 1u) {
                    uint32_t c = st + 0x127D0u + i * 0x180u;
                    float dx = S4_MF(c) - S4_MF(h + 0x40u), dz = S4_MF(c + 8u) - S4_MF(h + 0x48u);
                    if (dx * dx + dz * dz < 250.0f * 250.0f)
                        cnt++;
                }
            fprintf(s_log, " %u", cnt);
        }
    }
    fprintf(s_log, " | view");
    for (k = 0; k < SPLIT4_MAX_PLAYERS; k++)
        fprintf(s_log, " %d", (int32_t)S4_M32(0x00793000u + k * 0x1CCu));
    /* Each player's controller record (setup + 0: 0x4AE724 + 0x188 n, the
     * pad of the title's port n). */
    fprintf(s_log, " | pad");
    for (k = 0; k < SPLIT4_MAX_PLAYERS; k++)
        fprintf(s_log, " %d", (int)(S4_M32(0x0078A000u + k * 0x24u) - 0x004AE724u) / 0x188);
    fputc('\n', s_log);
}

/* ---- The four-player stage (step 3, bring-up) --------------------------
 *
 * BO3_SPLIT4=1 turns the title's two-player stage into a four-player one:
 * four human racers (and at most two AI rivals, six cars in all), setup
 * records and cameras for players 3 and 4. Without it nothing here acts and
 * the two-player stage is the title's. Players 3 and 4 drive with the
 * title's pad ports 0 and 1 (RECOMP_USB_PADS=4: the third and fourth
 * controllers); BO3_SPLIT4_AI has the title's AI drive either or both
 * instead (split4_ai34). */

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
#define S4_HUD           0x0054F900u   /* the HUD manager, its player records first */
#define S4_HUD_REC       0x70u
#define S4_PHYS          0x007A0000u   /* relocated human physics objects */
#define S4_PHYS_SIZE     0x4AD0u
#define S4_WINDOW        0x00793000u   /* relocated stream windows (viewdata) */
#define S4_WINDOW_SIZE   0x1CCu

static int s4_four_racing(void);
static int s4_skip(const char *part);
#define S4_CAR_AI        0x27D8u       /* byte: the AI drives this car */
#define S4_CAR_PHASE     0x134Cu       /* 2: in play */
#define S4_CAR_AI_RESET  0x19D0u
#define S4_CAR_DRIVER    0x1A00u

extern void sub_00015900(void);        /* the player camera's constructor (ecx) */
extern void sub_001673E0(void);        /* the player camera's init (ecx; eax the player) */
extern void sub_00167940(void);
extern void sub_0019CF20(void);        /* a stream window's frame (eax the window) */
extern uint32_t xbox_ContiguousAllocEx(uint32_t size, uint32_t alignment,
                                       uint32_t phys_lo, uint32_t phys_hi);        /* a player's cameras' frame (esi the camera; time, ret 4) */
static void s4_vcall(uint32_t obj, uint32_t off);
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

/* BO3_SPLIT4_AI: which of players 3 and 4 the title's AI drives, a bit
 * each: "1" (or "34") both, "3" player 3 only, "4" player 4 only; unset or
 * "0" neither (their pads drive). */
static int split4_ai34(void)
{
    if (s_ai < 0) {
        const char *v = getenv("BO3_SPLIT4_AI");
        s_ai = 0;
        if (v && *v && *v != '0') {
            if (strchr(v, '3'))
                s_ai |= 1;
            if (strchr(v, '4'))
                s_ai |= 2;
            if (!s_ai)
                s_ai = 3;
        }
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
            /* The app's init (sub_00015A60) gives cameras 0 and 1 their
             * player (sub_001673E0: ecx the camera, eax the index, at
             * + 0xC30). Cameras 2 and 3 would stay player 0's and take view
             * 0 at their bind; they get theirs, once. */
            if (S4_M32(cam + 0xC30u) != k) {
                uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp;
                g_ecx = cam;
                g_eax = k;
                PUSH32(g_esp, 0);
                sub_001673E0();
                g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
                fprintf(stderr, "[SPLIT4] camera %u set up for player %u\n", k, S4_M32(cam + 0xC30u));
            }
        }
        fprintf(stderr, "[SPLIT4] stage start: %u humans, %u AIs\n",
                S4_M32(mod + S4_STAGE_HUMANS), S4_M32(mod + S4_STAGE_AIS));
    }
    g_ecx = mod;
    gen();
}

/* The two-player stage's set-up (sub_000209A0) fills the setup records of
 * players 1 and 2: + 0 the controller record (0x4AE724 + 0x188 n, four, one
 * per pad port; record n reads port n) whose pad the car drives with, the
 * one 0x4AED45 names for the player (+ 0x18), and the flags. Players 3 and
 * 4 take the two records players 1 and 2 do not have, in port order (with
 * RECOMP_USB_PADS=4 the third and fourth controllers), and player 1's
 * flags. */
#define S4_CTLREC        0x004AE724u   /* the controller records, 0x188 each */
#define S4_CTLREC_SIZE   0x188u

void split4_stage_setup(void (*gen)(void))
{
    unsigned k, n, used = 0;
    gen();
    if (!split4_enabled() || !stage_is_2p())
        return;
    for (k = 0; k < 2; k++) {
        uint32_t rec = S4_M32(S4_SETUP + k * S4_SETUP_SIZE);
        if (rec >= S4_CTLREC && rec < S4_CTLREC + 4 * S4_CTLREC_SIZE)
            used |= 1u << ((rec - S4_CTLREC) / S4_CTLREC_SIZE);
    }
    for (k = 2, n = 0; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t r = S4_SETUP + k * S4_SETUP_SIZE;
        while (n < 3 && (used & (1u << n)))
            n++;
        used |= 1u << n;
        S4_W32(r + 0x00) = S4_CTLREC + n * S4_CTLREC_SIZE;
        S4_W32(r + 0x1C) = S4_M32(S4_SETUP + 0x1C);
        S4_W8(r + 0x20) = S4_M8(S4_SETUP + 0x20);
        S4_W8(r + 0x18) = (uint8_t)k;
        fprintf(stderr, "[SPLIT4] player %u drives with pad port %u\n", k + 1, n);
    }
}

/* ---- The HUD for players 3 and 4 ----
 *
 * The HUD manager (0x54F900) keeps a record per panel (0x70 each: a state,
 * the player at + 0x10, a drawable at + 0x14, the rectangle at + 0x48, the
 * widget list at + 0x68), two of them, and its own fields right after them
 * (+ 0xE0 .. + 0x1A0: colours many functions read at fixed addresses, the
 * panel count at + 0x170). Every manager method walks the count's records
 * from the manager it is given (update sub_000535E0, draw sub_000536C0,
 * reset sub_00053660, layout sub_00053700, states sub_00053620), so a
 * second manager (S4_HUD2, built by the title's constructor sub_000526C0)
 * holds players 3's and 4's panels in the bottom quarters.
 *
 * A widget is a static object (P1's from 0x3FD4A0, P2's from 0x3FE5F0)
 * with its manager at + 4 and its panel at + 0x1C; its creation function
 * takes the panel, which is also the player whose car it shows, and
 * registers it in the record manager + panel x 0x70. Players 3's and 4's
 * are copies of P1's block (S4_HUD_W3, S4_HUD_W4), created as the
 * two-player race creates P1's (sub_00052AB0: sub_00052D50's mode 2 case,
 * sub_00053420's, sub_00053540's) with panel 2 or 3 and the manager
 * S4_HUD2 - 0xE0 (so that the record is S4_HUD2's 0 or 1; the count the
 * creation reads there, + 0x170, is S4_HUD2 + 0x90, made 2 for the time),
 * then given S4_HUD2 and panel 0 or 1. Nothing else reads the panel (it is
 * only ever the record's index). */
#define S4_HUD2          0x007B4000u   /* players 3's and 4's HUD manager */
#define S4_HUD_W1        0x003FD4A0u   /* player 1's widget block */
#define S4_HUD_WSIZE     0x00001150u
#define S4_HUD_W3        0x007B5000u   /* player 3's copy */
#define S4_HUD_W4        0x007B6200u   /* player 4's */
#define S4_HUD_ARENAS    0x003EAF28u   /* the panels' drawable memory, by panel */
#define S4_HUD_ARENA_SIZE 0x00001880u
#define S4_HUD2_ARENA    0x007B8000u   /* S4_HUD2's two */

static int s_hud2;                     /* S4_HUD2 constructed */

extern void sub_00056DB0(void), sub_0004BFC0(void), sub_00059850(void),
    sub_00053ED0(void), sub_00051650(void), sub_00054630(void),
    sub_0004E100(void), sub_00053B70(void), sub_000538B0(void),
    sub_00053A10(void);

/* A guest call: these registers, these pushes (in push order), a return
 * address of 0; every register and esp as they were after. */
static void s4_gcall(void (*fn)(void), uint32_t eax, uint32_t ecx, uint32_t edx,
                     uint32_t ebx, uint32_t esi, uint32_t edi,
                     unsigned n, const uint32_t *push)
{
    uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp;
    unsigned i;
    for (i = 0; i < n; i++)
        PUSH32(g_esp, push[i]);
    PUSH32(g_esp, 0);
    g_eax = eax; g_ecx = ecx; g_edx = edx; g_ebx = ebx; g_esi = esi; g_edi = edi;
    fn();
    g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
}

/* A guest call with eax set and the other registers as they are; its eax
 * back in *r (when r). */
static void s4_gcall_r(void (*fn)(void), uint32_t eax, unsigned n, const uint32_t *push,
                       uint32_t *r)
{
    uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp;
    unsigned i;
    for (i = 0; i < n; i++)
        PUSH32(g_esp, push[i]);
    PUSH32(g_esp, 0);
    g_eax = eax;
    fn();
    if (r)
        *r = g_eax;
    g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
}

/* sub_000526C0 (edi the manager): the HUD manager's constructor, which
 * also gives every static widget its manager. S4_HUD2 is built first, so
 * the widgets end up with 0x54F900's as they should. */
void split4_hud_ctor(void (*gen)(void))
{
    if (split4_enabled() && g_edi == S4_HUD) {
        uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp;
        uint32_t t0 = S4_M32(S4_HUD_ARENAS), t1 = S4_M32(S4_HUD_ARENAS + 4u);
        /* each panel's drawable takes its memory (0x1880 bytes) from a
         * table by panel; S4_HUD2's get their own */
        S4_W32(S4_HUD_ARENAS) = S4_HUD2_ARENA;
        S4_W32(S4_HUD_ARENAS + 4u) = S4_HUD2_ARENA + S4_HUD_ARENA_SIZE;
        g_edi = S4_HUD2;
        gen();                       /* its ret pops the return address; put back */
        g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
        S4_W32(S4_HUD_ARENAS) = t0;
        S4_W32(S4_HUD_ARENAS + 4u) = t1;
        s_hud2 = 1;
    }
    gen();
}

/* The widgets of a copy of P1's block at w, panel k. */
static uint32_t s4_w(uint32_t w, uint32_t a) { return w + (a - S4_HUD_W1); }

static const uint32_t s4_hud_widgets[] = {
    0x003FDC00u, 0x003FD550u, 0x003FDD48u, 0x003FD4A0u, 0x003FD4F8u,
    0x003FE268u, 0x003FE4B8u,
};

static void s4_hud_build_player(unsigned k)
{
    uint32_t w = k == 2u ? S4_HUD_W3 : S4_HUD_W4;
    uint32_t mtmp = S4_HUD2 - 2u * S4_HUD_REC, h = SPLIT4_HUMAN + k * SPLIT4_HUMAN_SIZE;
    uint8_t *mem = (uint8_t *)(uintptr_t)g_xbox_mem_offset;
    unsigned i;

    memcpy(mem + w, mem + S4_HUD_W1, S4_HUD_WSIZE);
    for (i = 0; i < sizeof s4_hud_widgets / sizeof s4_hud_widgets[0]; i++) {
        S4_W32(s4_w(w, s4_hud_widgets[i]) + 4u) = mtmp;
        S4_W32(s4_w(w, s4_hud_widgets[i]) + 0xCu) = 0;
    }
    /* sub_00052D50, mode 2 (0x530DF), player 1's calls with panel k */
    { uint32_t a[] = { k };
      s4_gcall(sub_00056DB0, 1, 0, 0x1E, 0, s4_w(w, 0x3FDC00u), 0, 1, a); }
    { uint32_t a[] = { 0x3FF2C8u, 1, 0, k };
      s4_gcall(sub_0004BFC0, 1, 3, 0, s4_w(w, 0x3FD550u), 0, 0, 4, a); }
    { uint32_t a[] = { k };
      s4_gcall(sub_00059850, 1, 5, 0, s4_w(w, 0x3FDD48u), 0, 0, 1, a); }
    { uint32_t a[] = { 1, 0, 0, k };
      s4_gcall(sub_00053ED0, 1, 1, 0, s4_w(w, 0x3FD4A0u), 0, 0, 4, a); }
    if (S4_M8(0x0073A198u) == 0) {
        uint32_t a[] = { 1, s4_w(w, 0x3FD4F8u) };
        s4_gcall(sub_00051650, 1, 7, k, 0, 0, 0, 2, a);
    }
    /* sub_00053420, mode 2 (0x53441) */
    { uint32_t a[] = { 0 };
      s4_gcall(sub_00054630, 7, 8, k, 0, s4_w(w, 0x3FE268u), 0, 1, a); }
    /* sub_00053540, mode 2: a widget bound to the human by hand */
    { uint32_t a[] = { 8, 5, 1, 1, s4_w(w, 0x3FE4B8u) }, x = s4_w(w, 0x3FE4B8u);
      s4_gcall(sub_0004E100, k, 1, 0, 0, 0, 0, 5, a);
      S4_W32(x + 0x28u) = h;
      S4_W32(x + 0x24u) = h + 0x16D0u;
      S4_W32(x + 0x2Cu) = S4_M32(0x003B16C0u);
      S4_W32(x + 0x20u) = 0; }
    /* their own manager and panel from now on */
    for (i = 0; i < sizeof s4_hud_widgets / sizeof s4_hud_widgets[0]; i++) {
        S4_W32(s4_w(w, s4_hud_widgets[i]) + 4u) = S4_HUD2;
        S4_W8(s4_w(w, s4_hud_widgets[i]) + 0x1Cu) = (uint8_t)(k - 2u);
    }
}

/* sub_00052AB0 (ebx the manager): the race's HUD build; for the two-player
 * race, P1's and P2's widgets in 0x54F900's panels. With four racing,
 * players 3's and 4's in S4_HUD2's after it. */
void split4_hud_build(void (*gen)(void))
{
    uint32_t m = g_ebx, k, r, n, sv;
    gen();
    if (m != S4_HUD || !s_hud2 || !s4_four_racing() || S4_M32(m + 0x170u) != 2u
        || s4_skip("hud2"))
        return;
    /* the manager's fields as 0x54F900's (count, mode, state) */
    memcpy((uint8_t *)(uintptr_t)g_xbox_mem_offset + S4_HUD2 + 0x170u,
           (const uint8_t *)(uintptr_t)g_xbox_mem_offset + S4_HUD + 0x170u, 0x10u);
    S4_W8(S4_HUD2 + 0xE0u) = 0;
    /* the panels emptied and reset, as the build does 0x54F900's */
    for (k = 0; k < 2u; k++) {
        r = S4_HUD2 + k * S4_HUD_REC;
        for (n = S4_M32(r + 0x68u); n; ) {
            uint32_t next = S4_M32(n + 0xCu);
            S4_W32(n + 0xCu) = 0;
            n = next;
        }
        S4_W32(r + 0x68u) = 0;
        s4_gcall(sub_000538B0, r, 0, 0, 0, 0, 0, 0, NULL);
        s4_gcall(sub_00053B70, 0, 0, 0, 0, 0, r, 0, NULL);
    }
    sv = S4_M32(S4_HUD2 + 0x90u);
    S4_W32(S4_HUD2 + 0x90u) = 2;     /* the count the creation reads */
    s4_hud_build_player(2);
    s4_hud_build_player(3);
    S4_W32(S4_HUD2 + 0x90u) = sv;
    /* the build's last step: each panel shown (state 5, sub_00053A10) */
    for (k = 0; k < 2u; k++) {
        uint32_t one = 0x3F800000u;
        r = S4_HUD2 + k * S4_HUD_REC;
        S4_W32(r + 4u) = 0;
        S4_W32(r + 8u) = 5;
        s4_gcall(sub_00053A10, 0, 0, 0, 0, 0, r, 1, &one);
        S4_W32(r + 8u) = 0;
        S4_W32(r + 4u) = 0;
    }
    S4_W32(S4_HUD2 + 0x178u) = 0x17;
    fprintf(stderr, "[SPLIT4] HUD for players 3 and 4 built\n");
}

/* The manager methods that walk the panels (ebx the manager): after
 * 0x54F900's, S4_HUD2's. update: sub_000535E0 (its time on the stack,
 * ret 4); draw: sub_000536C0; reset: sub_00053660. */
static void s4_hud_again(void (*gen)(void), int arg, int racing)
{
    uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp;
    if (!s_hud2 || (racing && !s4_four_racing()))
        return;
    if (arg)
        PUSH32(g_esp, S4_M32(sp - 4u));     /* the argument the first call had */
    PUSH32(g_esp, 0);
    g_ebx = S4_HUD2;
    gen();
    g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
}

void split4_hud_update(void (*gen)(void))
{
    uint32_t m = g_ebx;
    gen();                                  /* ret 4: esp is past its argument */
    if (m == S4_HUD)
        s4_hud_again(gen, 1, 1);
}

void split4_hud_draw(void (*gen)(void))
{
    uint32_t m = g_ebx;
    gen();
    if (m == S4_HUD)
        s4_hud_again(gen, 0, 1);
}

void split4_hud_reset(void (*gen)(void))
{
    uint32_t m = g_ebx;
    gen();
    if (m == S4_HUD && split4_enabled())
        s4_hud_again(gen, 0, 0);
}

/* sub_00053620 (the manager on the stack, ret 4): every panel to state 1
 * (a car's start). */
void split4_hud_states(void (*gen)(void))
{
    uint32_t m = S4_M32(g_esp + 4u);
    gen();
    if (m == S4_HUD && s_hud2 && s4_four_racing()) {
        uint32_t a[] = { S4_HUD2 };
        s4_gcall(gen, g_eax, g_ecx, g_edx, g_ebx, g_esi, g_edi, 1, a);
    }
}

/* sub_00053D20 (edi a panel, eax the state), called for a player's panel
 * by its index (0x54F900 + player x 0x70: sub_00027920, sub_00027AD0,
 * sub_0018C510, sub_0018C7D0, sub_0018C820). Players 3's and 4's would be
 * the manager's own fields: they go to S4_HUD2's panels (dropped when it
 * is not there). */
void split4_hud_state(void (*gen)(void))
{
    uint32_t r = g_edi;
    if (split4_enabled() && r >= S4_HUD + 2u * S4_HUD_REC && r < S4_HUD + 4u * S4_HUD_REC) {
        if (!s_hud2 || !s4_four_racing()) {
            g_esp += 4u;                         /* its ret */
            return;
        }
        g_edi = S4_HUD2 + (r - S4_HUD - 2u * S4_HUD_REC);
        gen();
        g_edi = r;
        return;
    }
    gen();
}

/* sub_00053700 (eax the HUD manager, ecx the panel count): with two, the
 * title's split layout, panel 0 the top half and panel 1 the bottom half,
 * each a rectangle (+0x48 x0, +0x4C y0, +0x50 x1, +0x54 y1; panel 1's at
 * + 0x70) with the title's margins (the gap at the middle is the top
 * margin). With four racing, panels 0 and 1 become the top-left and
 * top-right quarters, players 1's and 2's views, with the same gap at the
 * vertical middle; S4_HUD2's two (players 3 and 4) the bottom ones. */
void split4_hud_layout(void (*gen)(void))
{
    uint32_t m = g_eax, n = g_ecx;
    float x0, y0, x1, ym, g, xm, yb;
    gen();
    if (n != 2u || m != S4_HUD || !s4_four_racing() || s4_skip("hud"))
        return;
    x0 = S4_MF(m + 0x48u);
    y0 = S4_MF(m + 0x4Cu);
    x1 = S4_MF(m + 0x50u);
    g = (S4_MF(m + 0x70u + 0x4Cu) - S4_MF(m + 0x54u)) * 0.5f;
    ym = S4_MF(m + 0x54u) + g;
    xm = (x0 + x1) * 0.5f;
    yb = S4_MF(m + 0x70u + 0x54u);
    S4_WF(m + 0x48u) = x0;       S4_WF(m + 0x4Cu) = y0;
    S4_WF(m + 0x50u) = xm - g;   S4_WF(m + 0x54u) = ym - g;
    S4_WF(m + 0xB8u) = xm + g;   S4_WF(m + 0xBCu) = y0;
    S4_WF(m + 0xC0u) = x1;       S4_WF(m + 0xC4u) = ym - g;
    /* players 3's and 4's panels: S4_HUD2's, the bottom quarters */
    if (s_hud2) {
        uint32_t a = g_eax, c = g_ecx, sp = g_esp;
        g_eax = S4_HUD2;
        g_ecx = 2;
        PUSH32(g_esp, 0);
        gen();
        g_eax = a; g_ecx = c; g_esp = sp;
        m = S4_HUD2;
        S4_WF(m + 0x48u) = x0;       S4_WF(m + 0x4Cu) = ym + g;
        S4_WF(m + 0x50u) = xm - g;   S4_WF(m + 0x54u) = yb;
        S4_WF(m + 0xB8u) = xm + g;   S4_WF(m + 0xBCu) = ym + g;
        S4_WF(m + 0xC0u) = x1;       S4_WF(m + 0xC4u) = yb;
    }
}

/* The player camera's mode switch (sub_001B4230, the cameras' vtable + 0xC:
 * ecx the camera, the mode and a target on the stack, ret 8). The race's
 * global switches (the intro, the start, the finish, the replays) name the
 * two cameras one after the other, camera 0 then camera 1, from these
 * places (their return addresses); cameras 2 and 3 take camera 1's switch
 * there too. A switch for one player (a crash, a takedown) indexes its
 * camera and is left alone. The world load's (0x1AA154) is
 * split4_world_load's. */
static const uint32_t s4_cam_sites[] = {
    0x000247ACu, 0x0002489Du, 0x000248B6u, 0x00024926u, 0x00026014u,
    0x000261C9u, 0x00026968u, 0x00026A5Eu, 0x00026CFCu, 0x0002723Fu,
    0x00072F48u,
};

void split4_camera_mode(void (*gen)(void))
{
    uint32_t cam = g_ecx, sp = g_esp, ret = S4_M32(sp);
    uint32_t mode = S4_M32(sp + 4u), arg = S4_M32(sp + 8u);
    unsigned i, k;
    gen();
    if (cam != S4_CAM + S4_CAM_SIZE || !s4_four_racing())
        return;
    for (i = 0; i < sizeof s4_cam_sites / sizeof s4_cam_sites[0]; i++)
        if (s4_cam_sites[i] == ret)
            break;
    if (i == sizeof s4_cam_sites / sizeof s4_cam_sites[0])
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, s2 = g_esp;
        g_ecx = S4_CAM + k * S4_CAM_SIZE;
        PUSH32(g_esp, arg);
        PUSH32(g_esp, mode);
        PUSH32(g_esp, 0);
        gen();
        g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = s2;
    }
}

/* config/patches.json s4_cam_frame: the app's frame updates the players'
 * cameras 0 and 1 by name (sub_000170B0); cameras 2 and 3 are updated
 * after them, with the frame's time (app + 0x2E210). Without it they keep
 * the pose they were built with and views 3 and 4 look at the sky. */
/* A player camera's chosen view (+ 0xC3C: 2 or 5, the two chase views the
 * player toggles in the race), set for cameras 0 and 1 from the players'
 * options (sub_00018910, sub_00020800) and 0 for 2 and 3: with 0 their
 * camera sat under the car or far from it. Players 3 and 4 start with
 * player 1's. */
#define S4_CAM_VIEW      0x0C3Cu

static void s4_cam_view(unsigned k)
{
    uint32_t cam = S4_CAM + k * S4_CAM_SIZE, v = S4_M32(cam + S4_CAM_VIEW);
    if (v != 2u && v != 5u) {
        v = S4_M32(S4_CAM + S4_CAM_VIEW);
        S4_W32(cam + S4_CAM_VIEW) = (v == 2u || v == 5u) ? v : 2u;
    }
}

static void split4_cam_frame(void)
{
    unsigned k;
    float dt;
    if (!s4_four_racing())
        return;
    dt = S4_MF(g_edi + 0x2E210u);
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp;
        s4_cam_view(k);
        g_esi = S4_CAM + k * S4_CAM_SIZE;
        g_esp -= 8u;
        S4_W32(g_esp) = 0;                         /* the return address */
        S4_WF(g_esp + 4u) = dt;                    /* its one argument, ret 4 */
        sub_00167940();
        g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
    }
}

/* s4_cam_reset: sub_00016D00 resets cameras 0 and 1 by name (vtable + 4);
 * cameras 2 and 3 too. */
static void split4_cam_reset(void)
{
    unsigned k;
    if (!s4_four_racing())
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        s4_cam_view(k);
        s4_vcall(S4_CAM + k * S4_CAM_SIZE, 4u);
    }
}

/* s4_env_view: the object at 0x60E040 keeps an entry per view (+ 0xB0,
 * 0x40 each) for two views, its current view (+ 0x130) right after them.
 * Views 2 and 3 take entries 0 and 1. */
static void split4_env_view(void)
{
    uint32_t v = S4_M32(0x0060E170u);
    if (v >= 2u && v < SPLIT4_MAX_PLAYERS)
        S4_W32(0x0060E170u) = v & 1u;
}

/* The traffic's start (sub_001A28B0, the race world's traffic at the GO):
 * for each human (the count at world + 0x12B7C0) it writes the human's
 * track section and spawn count into the streamer's two-entry arrays
 * (streamer + 0x363A7 and + 0x363A9, one byte a focus: the streamer has two
 * foci, players 1 and 2). Players 3 and 4 wrote past them, over the traffic
 * car count (+ 0x363AB) and its neighbour, and no traffic came. It runs for
 * two. */
static int s_traffic34_set;           /* split4_streamer's players 3 and 4 */

void split4_traffic_init(void (*gen)(void))
{
    uint32_t n = S4_M32(SPLIT4_WORLD + 0x12B7C0u);

    if (!split4_enabled() || n <= 2u) {
        gen();
        return;
    }
    S4_W32(SPLIT4_WORLD + 0x12B7C0u) = 2u;
    gen();
    S4_W32(SPLIT4_WORLD + 0x12B7C0u) = n;
    s_traffic34_set = 0;
}

/* The streamer's frame (sub_001A20F0, ebx the streamer at world + 0x4DE0):
 * traffic spawns around its two foci, players 1 and 2 (sub_001A3110 for
 * each: the focus's track section in ecx; the focus's zone + 0x363A7 and
 * spawn count + 0x363A9, a byte each). With four racing, after it players
 * 3 and 4 get the same call, as foci 0 and 1 with their own two bytes
 * swapped in (s_traffic34, first player 1's and 2's). Their traffic joins
 * the one pool and moves and leaves as any (by every racer's distance). */
extern void sub_001A3110(void);
static uint8_t s_traffic34[2][2];      /* players 3's and 4's zone, count */

static uint32_t s4_vcall_r(uint32_t obj, uint32_t off)
{
    uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp, r = 0;
    recomp_func_t fn = recomp_lookup(S4_M32(S4_M32(obj) + off));
    if (fn) {
        g_ecx = obj;
        PUSH32(g_esp, 0);
        fn();
        r = g_eax;
    }
    g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
    return r;
}

void split4_streamer(void (*gen)(void))
{
    uint32_t st = g_ebx, k;
    gen();
    if (!s4_four_racing() || s4_skip("traffic34"))
        return;
    if (!S4_M8(st + 0x363BBu) || S4_M8(st + 0x363BDu) || S4_M8(st + 0x363B8u) || S4_M8(st + 0x363B7u)
        || S4_M8(st + 0x363ABu) == 0)
        return;
    if (!s_traffic34_set) {
        for (k = 0; k < 2u; k++) {
            s_traffic34[k][0] = S4_M8(st + 0x363A7u + k);
            s_traffic34[k][1] = 0;
        }
        s_traffic34_set = 1;
    }
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t h = SPLIT4_HUMAN + k * SPLIT4_HUMAN_SIZE, j = k - 2u, sec, dt = S4_M32(0x0060EA20u);
        uint8_t z = S4_M8(st + 0x363A7u + j), n = S4_M8(st + 0x363A9u + j);
        if (S4_M32(h) != S4_HUMAN_VT)
            continue;
        sec = s4_vcall_r(h, 0x18u) & 0xFFFFu;
        S4_W8(st + 0x363A7u + j) = s_traffic34[j][0];
        S4_W8(st + 0x363A9u + j) = s_traffic34[j][1];
        {
            uint32_t a[] = { j, dt, st };
            s4_gcall(sub_001A3110, 0, sec, 0, st, j, 0, 3, a);
        }
        s_traffic34[j][0] = S4_M8(st + 0x363A7u + j);
        s_traffic34[j][1] = S4_M8(st + 0x363A9u + j);
        S4_W8(st + 0x363A7u + j) = z;
        S4_W8(st + 0x363A9u + j) = n;
    }
}

/* The car update mark (ap_update, sub_0018C510, esi the car): with
 * BO3_SPLIT4_AI the title's AI takes players 3 and 4 once they are in play,
 * as autopilot.c does for player 1. */
static void split4_car_update(void)
{
    uint32_t car = g_esi;

    if (!((car == SPLIT4_HUMAN + 2 * SPLIT4_HUMAN_SIZE && (split4_ai34() & 1)) ||
          (car == SPLIT4_HUMAN + 3 * SPLIT4_HUMAN_SIZE && (split4_ai34() & 2))))
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

/* The track's stream windows (world + 0x12ADC8, relocated to S4_WINDOW,
 * 0x1CC each). Window k streams the track sections around human k (its
 * + 0x1C; sub_0019CF20 follows that human's section) into buffers from a
 * pool (0x739B60: + 0x10 19 large slots, + 0x5C 35 small, filled from
 * descriptor tables at fixed addresses), and a car finds its ground in the
 * window its view byte names (physics + 0x217, + 0x4ACC). The title runs
 * two: the world's load starts window 1 only for exactly two humans (marks
 * s4_stream_*: esi, the 2 it compares the count with, becomes the count),
 * and each frame sub_001AA920 updates windows 0 and 1.
 *
 * Windows 2 and 3 (players 3 and 4) run beside them with a pool each: the
 * pool's slot arrays and the two descriptor tables are swapped with the
 * window's own set (S4_POOL2_*, its descriptors for buffers this code
 * allocates once, 6.5 MB for the two) around every call the title makes on window 1
 * that has to be made on them too: init (sub_0019C6E0), start
 * (sub_0019C740), the frame's update (sub_0019CF20 from sub_001AA920),
 * release (sub_0019CA30). The streamer's own foci (its zones of props and
 * textures, sub_001A20F0) stay players 1 and 2. */
#define S4_POOL          0x00739B60u   /* the section buffer pool */
#define S4_POOL_SLOTS    0x10u         /* + 0x10 .. + 0xE8: 19 + 35 slots */
#define S4_POOL_SLOTS_N  0xD8u
#define S4_POOL_A_N      19u
#define S4_POOL_B_N      35u
#define S4_POOL_A_SIZE   0x28000u
#define S4_POOL_B_SIZE   0x18000u
#define S4_POOL2_SLOTS   0x007BC000u   /* windows 2's and 3's slot arrays, */
#define S4_POOL2_TAB_A   0x007BC100u   /*   descriptors (16 bytes each), */
#define S4_POOL2_TAB_B   0x007BC300u   /*   one set each, S4_POOL2_STRIDE apart */
#define S4_POOL2_STRIDE  0x00000800u
#define S4_WIN_A         10u           /* what one window takes: + 8, + 0xC */
#define S4_WIN_B         18u
#define S4_WIN_ROUNDS    4u            /* extra window steps a frame, four players */
#define S4_WIN_LOCK      0x00494EF8u   /* the window using the reader, -1 none */
#define S4_POOL_MODE     0x003FA644u   /* 2: the tables below; else 0x3F9E44/0x3F9EE4 */

static uint32_t s_pool2_a, s_pool2_b;  /* their buffers, window 2's then 3's */
static int s_win_started[SPLIT4_MAX_PLAYERS];
static int s_win1_ready;              /* window 1 started, 2 and 3 still starting */

static int split4_more(void)
{
    return S4_M32(SPLIT4_WORLD + 0x12B7C0u) > 2u;
}

static uint32_t s4_win(unsigned k) { return S4_WINDOW + k * S4_WINDOW_SIZE; }

static uint32_t s4_tab_a(void)
{
    return S4_M32(S4_POOL_MODE) == 2u ? 0x003FA164u : 0x003F9E44u;
}

static uint32_t s4_tab_b(void)
{
    return S4_M32(S4_POOL_MODE) == 2u ? 0x003FA294u : 0x003F9EE4u;
}

static void s4_swap(uint32_t x, uint32_t y, uint32_t n)
{
    uint8_t *m = (uint8_t *)(uintptr_t)g_xbox_mem_offset, t;
    uint32_t i;
    for (i = 0; i < n; i++) {
        t = m[x + i]; m[x + i] = m[y + i]; m[y + i] = t;
    }
}

/* Window k's pool (2 or 3) in, or back out. */
static void s4_pool_swap(unsigned k)
{
    uint32_t o = (k - 2u) * S4_POOL2_STRIDE;
    s4_swap(S4_POOL + S4_POOL_SLOTS, S4_POOL2_SLOTS + o, S4_POOL_SLOTS_N);
    s4_swap(s4_tab_a(), S4_POOL2_TAB_A + o, S4_POOL_A_N * 16u);
    s4_swap(s4_tab_b(), S4_POOL2_TAB_B + o, S4_POOL_B_N * 16u);
}

/* Windows 2's and 3's pools, every buffer free (the buffers allocated the
 * first time): each the title's layout (19 large and 35 small slots), with
 * buffers behind the ten and eighteen one window takes and the rest of the
 * descriptors taken, so a window always has its spare. 0 when the memory
 * is not there. */
static int s4_pool2_reset(void)
{
    uint32_t i, k, o, t, a, b;
    if (!s_pool2_a) {
        /* above 32 MB: the arena's low pages include the guest stack's
         * (0x840000.. with this image), which it does not know about */
        s_pool2_a = xbox_ContiguousAllocEx(2u * S4_WIN_A * S4_POOL_A_SIZE, 0x1000u,
                                           0x02000000u, 0x03FFFFFFu);
        s_pool2_b = xbox_ContiguousAllocEx(2u * S4_WIN_B * S4_POOL_B_SIZE, 0x1000u,
                                           0x02000000u, 0x03FFFFFFu);
        fprintf(stderr, "[SPLIT4] track buffers for players 3 and 4: %08X, %08X\n",
                s_pool2_a, s_pool2_b);
    }
    if (!s_pool2_a || !s_pool2_b)
        return 0;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        o = (k - 2u) * S4_POOL2_STRIDE;
        a = s_pool2_a + (k - 2u) * S4_WIN_A * S4_POOL_A_SIZE;
        b = s_pool2_b + (k - 2u) * S4_WIN_B * S4_POOL_B_SIZE;
        for (i = 0; i < S4_POOL_SLOTS_N; i += 4)
            S4_W32(S4_POOL2_SLOTS + o + i) = 0;
        for (i = 0; i < S4_POOL_A_N; i++) {   /* free (0) or taken, any mode, size, buffer */
            t = S4_POOL2_TAB_A + o + i * 16u;
            S4_W32(t) = i < S4_WIN_A ? 0 : 0xFFFFFFFFu;
            S4_W32(t + 4u) = 0xFFFFFFFFu;
            S4_W32(t + 8u) = S4_POOL_A_SIZE;
            S4_W32(t + 12u) = i < S4_WIN_A ? a + i * S4_POOL_A_SIZE : 0;
        }
        for (i = 0; i < S4_POOL_B_N; i++) {
            t = S4_POOL2_TAB_B + o + i * 16u;
            S4_W32(t) = i < S4_WIN_B ? 0 : 0xFFFFFFFFu;
            S4_W32(t + 4u) = 0xFFFFFFFFu;
            S4_W32(t + 8u) = S4_POOL_B_SIZE;
            S4_W32(t + 12u) = i < S4_WIN_B ? b + i * S4_POOL_B_SIZE : 0;
        }
    }
    return 1;
}

/* A window frees a buffer by marking its descriptor 2 (sub_0019DAD0,
 * sub_0019DB90); only a 0 is taken again (sub_00018BB0). The title's own
 * descriptors come back to 0 by a pass this code does not run for windows
 * 2's and 3's: theirs are made 0 here, S4_POOL2_DELAY frames after (the
 * frames in flight may still draw from the buffer). */
#define S4_POOL2_DELAY   3u
static uint8_t s_pool2_age[2][S4_POOL_A_N + S4_POOL_B_N];

static void s4_pool2_release(void)
{
    unsigned k, i;
    for (k = 0; k < 2u; k++)
        for (i = 0; i < S4_POOL_A_N + S4_POOL_B_N; i++) {
            uint32_t t = i < S4_POOL_A_N
                ? S4_POOL2_TAB_A + k * S4_POOL2_STRIDE + i * 16u
                : S4_POOL2_TAB_B + k * S4_POOL2_STRIDE + (i - S4_POOL_A_N) * 16u;
            if (S4_M32(t) != 2u) {
                s_pool2_age[k][i] = 0;
                continue;
            }
            if (++s_pool2_age[k][i] >= S4_POOL2_DELAY) {
                S4_W32(t) = 0;
                s_pool2_age[k][i] = 0;
            }
        }
}

static void split4_stream_cmp(void)
{
    uint32_t step = S4_M32(SPLIT4_WORLD + 0x144384u);   /* the load step */
    if (split4_more() && (step == 0x10u || step == 0x11u))
        g_esi = S4_M32(SPLIT4_WORLD + 0x12B7C0u);
}

static void split4_stream_end(void)
{
    g_esi = 2;
}

/* A window call for window k (2 or 3), its pool in for it. */
static uint32_t s4_win_call(unsigned k, void (*fn)(void), uint32_t eax, unsigned n,
                            const uint32_t *push)
{
    uint32_t r;
    s4_pool_swap(k);
    s4_gcall_r(fn, eax, n, push, &r);
    s4_pool_swap(k);
    return r;
}

/* Human k's car finds its ground in window w (its view byte, physics
 * + 0x217 and + 0x4ACC). */
static void s4_car_window(unsigned k, unsigned w)
{
    uint32_t phys = S4_PHYS + k * S4_PHYS_SIZE;
    S4_W8(phys + 0x217u) = (uint8_t)w;
    S4_W32(phys + 0x4ACCu) = w;
}

/* sub_0019C6E0 (eax a window): the world's init of windows 0 and 1; with
 * window 1, windows 2 and 3 too, and their pool reset. */
void split4_win_init(void (*gen)(void))
{
    uint32_t w = g_eax;
    unsigned k;
    gen();
    if (!split4_enabled() || w != s4_win(1))
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        s4_gcall_r(gen, s4_win(k), 0, NULL, NULL);
        s_win_started[k] = 0;
    }
    s_win1_ready = 0;
    s4_pool2_reset();
}

/* sub_0019C740 (the window, a flag, its human on the stack; ret 12): the
 * world's load starts a window, call after call until it says it is
 * ready (al). With window 1 and more than two humans, windows 2 and 3
 * start the same way (players 3 and 4), and window 1 is ready when they
 * all are. */
void split4_win_start(void (*gen)(void))
{
    uint32_t w = S4_M32(g_esp + 4u), a1 = S4_M32(g_esp + 8u), ok, k;
    int ours = split4_enabled() && w == s4_win(1) && split4_more() && s_pool2_a;
    /* A window that said it is ready is running (+ 0x10 = 2), and a start
     * call on a running window restarts it: once window 1 is ready it is
     * not called again while windows 2 and 3 finish (its ret 12). */
    if (ours && s_win1_ready) {
        g_esp += 16u;
        g_eax |= 1u;
    } else {
        gen();
        if (!ours)
            return;
        s_win1_ready = (g_eax & 0xFFu) != 0;
    }
    /* one at a time, as the title starts its windows: they share the
     * disc reader */
    ok = s_win1_ready ? 1u : 0u;
    for (k = 2; k < SPLIT4_MAX_PLAYERS && k < S4_M32(SPLIT4_WORLD + 0x12B7C0u); k++) {
        if (!ok)
            break;
        if (!s_win_started[k]) {
            uint32_t push[] = { k, a1, s4_win(k) }, lock = S4_M32(S4_WIN_LOCK);
            if (S4_M32(s4_win(k) + 0x10u) == 0)     /* never put in its start state */
                S4_W32(s4_win(k) + 0x10u) = 1;
            /* the reader is one at a time: windows 0's and 1's frames take
             * it by this lock (sub_0019CF20); a start holds it till done */
            if (lock != 0xFFFFFFFFu && lock != k) {
                ok = 0;
                break;
            }
            S4_W32(S4_WIN_LOCK) = k;
            s_win_started[k] = (s4_win_call(k, gen, g_eax, 3, push) & 0xFFu) != 0;
            if (s_win_started[k]) {
                S4_W32(S4_WIN_LOCK) = 0xFFFFFFFFu;
                s4_car_window(k, k);
            }
        }
        ok = s_win_started[k] ? 1u : 0u;
    }
    g_eax = (g_eax & ~0xFFu) | ok;
    if (ok)
        s_win1_ready = 0;
}

/* sub_001AABD0 (esi the world): the stage's reset sets windows 0's and
 * 1's state (+ 0x10, to 0x19; they keep streaming, sub_0019CF20 does not
 * read it); windows 2 and 3 follow window 1. */
void split4_win_restart(void (*gen)(void))
{
    uint32_t st = S4_M32(s4_win(1) + 0x10u), k;
    gen();
    if (!split4_enabled() || S4_M32(s4_win(1) + 0x10u) == st)
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++)
        if (s_win_started[k])
            S4_W32(s4_win(k) + 0x10u) = S4_M32(s4_win(1) + 0x10u);
}

/* sub_0019CA30 (the window on the stack, ret 4): the world's release of
 * windows 0 and 1; windows 2 and 3 after window 1. */
void split4_win_release(void (*gen)(void))
{
    uint32_t w = S4_M32(g_esp + 4u), k;
    gen();
    if (!split4_enabled() || w != s4_win(1) || !s_pool2_a)
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t push[] = { s4_win(k) };
        s4_win_call(k, gen, g_eax, 1, push);
        s_win_started[k] = 0;
    }
}

/* sub_001AA920 (esi the world): each frame, windows 0 and 1 updated
 * (sub_0019CF20) and the world told whether they are still loading. With
 * more than two humans the count reads 2 for it (`cmp [0x73A1C0], 2`:
 * window 1 updated), then windows 2 and 3 are updated. */
void split4_stream_ready(void (*gen)(void))
{
    uint32_t n = S4_M32(SPLIT4_WORLD + 0x12B7C0u), k;
    if (!split4_enabled() || n <= 2u) {
        gen();
        return;
    }
    S4_W32(SPLIT4_WORLD + 0x12B7C0u) = 2;
    gen();
    S4_W32(SPLIT4_WORLD + 0x12B7C0u) = n;
    for (k = 2; k < SPLIT4_MAX_PLAYERS && k < n; k++)
        if (s_win_started[k])
            s4_win_call(k, sub_0019CF20, s4_win(k), 0, NULL);
    s4_pool2_release();
    /* A window loads a section in about four steps of its state machine,
     * one step a call, and the windows take turns at the reader: four
     * windows would each move a section only every twenty frames or so,
     * slower than a fast car crosses them. More steps a frame (reads end
     * at once here). */
    {
        unsigned round;
        for (round = 0; round < S4_WIN_ROUNDS; round++)
            for (k = 0; k < SPLIT4_MAX_PLAYERS && k < n; k++) {
                if (k < 2u)
                    s4_gcall_r(sub_0019CF20, s4_win(k), 0, NULL, NULL);
                else if (s_win_started[k])
                    s4_win_call(k, sub_0019CF20, s4_win(k), 0, NULL);
            }
    }
}

/* A human car's physics init (sub_001177B0, ecx the physics object, its
 * + 0x1580 the human racer) gives the car its view: + 0x217 (byte) and
 * + 0x4ACC = (human != slot 0) ? 1 : 0. Players 3 and 4 take windows 2 and
 * 3 once those stream (and when their windows finish starting, as the
 * cars are built before them). */
void split4_car_view(void (*gen)(void))
{
    uint32_t phys = g_ecx, h, k;                /* thiscall: the object in ecx */
    gen();
    if (!split4_enabled())
        return;
    h = S4_M32(phys + 0x1580u);
    if (h < SPLIT4_HUMAN || h >= SPLIT4_HUMAN + SPLIT4_MAX_PLAYERS * SPLIT4_HUMAN_SIZE)
        return;
    k = (h - SPLIT4_HUMAN) / SPLIT4_HUMAN_SIZE;
    if (k >= 2u && s_win_started[k])
        s4_car_window(k, k);
}

/* A car's reset (sub_0011A8F0, esi the physics object; sub_0010EDF0, ecx,
 * which calls it) sets its view byte to 0. Players 3 and 4 go back to
 * their own window: a window follows its car only while the car is near
 * it, so a car looked up in another window leaves its own behind. */
static void s4_car_back(uint32_t phys)
{
    uint32_t k;
    if (phys < S4_PHYS || phys >= S4_PHYS + SPLIT4_MAX_PLAYERS * S4_PHYS_SIZE)
        return;
    k = (phys - S4_PHYS) / S4_PHYS_SIZE;
    if (k >= 2u && (phys - S4_PHYS) % S4_PHYS_SIZE == 0 && s_win_started[k])
        s4_car_window(k, k);
}

void split4_car_reset(void (*gen)(void))
{
    uint32_t phys = g_esi;
    gen();
    if (split4_enabled())
        s4_car_back(phys);
}

void split4_car_reset2(void (*gen)(void))
{
    uint32_t phys = g_ecx;
    gen();
    if (split4_enabled())
        s4_car_back(phys);
}

/* The track contact's fallback view (config/patches.json s4_view_other_a /
 * _b): `mov al, 1 / sub al, <view>` takes "the other player's" window when
 * the car is not in its own: -1 or -2 for windows 2 and 3. Here the next
 * window is view ^ 1 (the title's 1 - view for 0 and 1; 3 and 2 for 2 and
 * 3), except that players 3 and 4 try their own window first. */
static uint32_t s4_view_next(uint32_t phys, uint32_t v)
{
    uint32_t k = (phys - S4_PHYS) / S4_PHYS_SIZE, t = v ^ 1u;
    /* players 3 and 4 back to their own window first (a car's reset sets
     * its view to 0, sub_0010EDF0) */
    if (phys >= S4_PHYS && phys < S4_PHYS + SPLIT4_MAX_PLAYERS * S4_PHYS_SIZE
        && k >= 2u && s_win_started[k] && v != k)
        t = k;
    return t;
}

/* al = view + next, so that the title's `sub al, view` gives next */
static void split4_view_other_a(void)
{
    uint32_t v = g_ebx & 0xFFu;                 /* edi the physics object */
    g_eax = (g_eax & ~0xFFu) | ((v + s4_view_next(g_edi, v)) & 0xFFu);
}

static void split4_view_other_b(void)
{
    uint32_t v = g_ecx & 0xFFu;                 /* ebx the physics object */
    g_eax = (g_eax & ~0xFFu) | ((v + s4_view_next(g_ebx, v)) & 0xFFu);
}

void split4_start(void)
{
    if (!split4_enabled())
        return;
    recomp_patch_on_mark("s4_stream_a", split4_stream_cmp);
    recomp_patch_on_mark("s4_stream_b", split4_stream_cmp);
    recomp_patch_on_mark("s4_stream_end", split4_stream_end);
    recomp_patch_on_mark("s4_view_other_a", split4_view_other_a);
    recomp_patch_on_mark("s4_view_other_b", split4_view_other_b);
    recomp_patch_on_mark("s4_cam_frame", split4_cam_frame);
    recomp_patch_on_mark("s4_cam_reset", split4_cam_reset);
    recomp_patch_on_mark("s4_env_view", split4_env_view);
    if (split4_ai34() && !recomp_env_on(RENV_AUTOPILOT))
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

/* ---- Four views (step 3) ------------------------------------------------
 *
 * The render manager (0x4D6170) holds the title's views: 2D at +0x310 and the
 * players' at +0x90 and +0x130 (0xA0 each), with an effects record each at
 * +0x1D0 and +0x270 (sub_00179FD0 fills them). sub_0002F330 makes one current
 * by a mode (0 the 2D view, 1 and 2 the players'): the manager's +0x3B0 and
 * +0x3B4 (0x4D6520 / 0x4D6524), and begins the view's RwCamera. The stage
 * render draws view i as mode i + 1 (sub_001AE340 -> sub_0003D9E0).
 *
 * Players 3 and 4 get views and effects records of their own in the block
 * after the image (the manager's next slots are the effects records), built
 * as the title builds its own (sub_0002ECC0 -> sub_001C8C10), made current as
 * modes 3 and 4, cut as quarters of the frame by the split builder, prepared
 * and drawn after the title's two (sub_001AE2F0). */

#define S4_MGR          0x004D6170u
#define S4_VIEW34       0x007B3000u   /* views of players 3 and 4, 0xA0 each */
#define S4_FX34         0x007B3200u   /* their effects records, 0xA0 each */
#define S4_VIEW_SIZE    0xA0u

extern RECOMP_TLS uint32_t g_ebx, g_edi;
extern void sub_001C8C10(void);        /* a view's set-up (esi the view; one arg, ret 4) */
extern void sub_001D9290(void);        /* RwCameraBeginUpdate (cdecl) */
extern void sub_001DE810(void);        /* RwRasterSubRaster (cdecl: sub, parent, rect) */
extern void sub_001AE340(void);        /* the stage render of one view (ebx the view; world, ret 4) */
extern void sub_001C8A60(void);        /* a view's matrices from its player camera (edi) */
extern void sub_00179FD0(void);        /* a view's effects record (eax the player, ebx the view; cdecl fx) */
extern void sub_0002F380(void);        /* the single-view builder (edi the manager; index, ret 4) */

/* BO3_SPLIT4_SKIP=<names>: development switch, the listed parts of the
 * four-view code stay off (views, select, quad, bind, render). */
static int s4_skip(const char *part)
{
    const char *v = getenv("BO3_SPLIT4_SKIP");
    return v && strstr(v, part) != NULL;
}

static uint32_t s4_view(unsigned k)
{
    return k < 2 ? S4_MGR + 0x90u + k * S4_VIEW_SIZE : S4_VIEW34 + (k - 2) * S4_VIEW_SIZE;
}

static uint32_t s4_fx(unsigned k)
{
    return k < 2 ? S4_MGR + 0x1D0u + k * S4_VIEW_SIZE : S4_FX34 + (k - 2) * S4_VIEW_SIZE;
}

static int s4_four_racing(void)
{
    return split4_enabled() && stage_is_2p() &&
           S4_M32(SPLIT4_WORLD + 0x12B7C0u) == SPLIT4_MAX_PLAYERS;
}

/* BO3_SPLIT4_KEYS=3 (or 4): while four race and nobody has paused, the
 * arrow keys drive player 3 (or 4) instead of player 1 (controls.c). Unset:
 * the first of the two the AI does not drive; 0: neither. The
 * title's pad port of that player (its controller record, setup + 0), or -1
 * when the arrows are player 1's as usual (menus, the pause menu). */
#define S4_PAUSED_BY     0x004D537Cu   /* the player who paused, -1 none */

int split4_keys_port(void)
{
    static int who = -1;
    uint32_t rec;

    if (who == -1) {
        const char *v = getenv("BO3_SPLIT4_KEYS");
        if (v && *v)
            who = *v == '3' ? 2 : *v == '4' ? 3 : -2;
        else    /* unset: the first of players 3 and 4 the AI does not drive */
            who = !(split4_ai34() & 1) ? 2 : !(split4_ai34() & 2) ? 3 : -2;
        if (who >= 0)
            fprintf(stderr, "[SPLIT4] the arrow keys drive player %d (BO3_SPLIT4_KEYS)\n", who + 1);
    }
    if (who < 0 || !s4_four_racing() || S4_M32(S4_PAUSED_BY) != 0xFFFFFFFFu)
        return -1;
    rec = S4_M32(S4_SETUP + (uint32_t)who * S4_SETUP_SIZE);
    if (rec < S4_CTLREC || rec >= S4_CTLREC + 4u * S4_CTLREC_SIZE)
        return -1;
    return (int)((rec - S4_CTLREC) / S4_CTLREC_SIZE);
}

/* sub_0002ECC0 (once, the manager on the stack): the title's views, then
 * those of players 3 and 4 the same way. */
void split4_views_create(void (*gen)(void))
{
    unsigned k;
    gen();
    if (!split4_enabled() || s4_skip("views"))
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t a = g_eax, bx = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp;
        g_esi = s4_view(k);
        g_ebx = 0;                     /* raster width and height: 0, a sub-raster */
        g_edi = 0;                     /*  of the frame (sub_0002ECC0 does the same) */
        PUSH32(g_esp, 0);              /* its argument (the title passes 0) */
        PUSH32(g_esp, 0);              /* the return address its ret 4 takes */
        sub_001C8C10();
        g_eax = a; g_ebx = bx; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
        S4_WF(s4_view(k) + 0x90u) = 1.0f;
    }
    fprintf(stderr, "[SPLIT4] views for players 3 and 4 built (0x%08X, 0x%08X)\n",
            s4_view(2), s4_view(3));
}

/* sub_0002F330 (eax the manager, ecx the mode): modes 3 and 4 are the views
 * of players 3 and 4. */
void split4_view_select(void (*gen)(void))
{
    uint32_t mgr = g_eax, mode = g_ecx;
    if (split4_enabled() && !s4_skip("select") && mgr == S4_MGR && (mode == 3 || mode == 4)) {
        uint32_t v = s4_view(mode - 1), sp;
        S4_W32(mgr + 0x3B0u) = v;
        S4_W32(mgr + 0x3B4u) = s4_fx(mode - 1);
        sp = g_esp;
        PUSH32(g_esp, S4_M32(v + 0x58u));   /* its RwCamera */
        PUSH32(g_esp, 0);
        sub_001D9290();
        g_ecx = S4_M32(v + 0x58u);           /* the title's pop ecx */
        g_esp = sp + 4u;                     /* the title's ret: its return address */
        return;
    }
    gen();
}

/* sub_0002F480 (esi the manager): the split views. With four racing, four
 * quarters: each view a sub-raster of the frame and depth buffers (+0x370,
 * +0x374), its size at +0x78 / +0x7C, the frame's +0x390 at +0x80, and the
 * aspect and scale a full-screen view has (+0x74, +0x70: taken from
 * sub_0002F380's view 0, so the widescreen setting's patches hold). */
void split4_split_build(void (*gen)(void))
{
    uint32_t mgr = g_esi, w, h, sp, rect, sp0 = g_esp;
    float aspect, scale;
    unsigned k;

    if (!s4_four_racing() || mgr != S4_MGR || s4_skip("quad")) {
        gen();
        return;
    }
    if (s4_skip("full")) {
        /* sub_0002F380's values for a full-screen view, without the call */
        int wide = S4_M8(mgr + 0xDu) != 0;
        aspect = wide ? 16.0f / 9.0f : 4.0f / 3.0f;
        scale = wide ? 1.2f : 1.0f;
    } else {   /* view 0 full screen, as the title's one-view stage does */
        uint32_t a = g_eax, c = g_ecx, d = g_edx, di = g_edi;
        sp = g_esp;
        g_edi = mgr;
        PUSH32(g_esp, 0);
        PUSH32(g_esp, 0);
        sub_0002F380();
        g_eax = a; g_ecx = c; g_edx = d; g_edi = di; g_esp = sp;
        scale = S4_MF(s4_view(0) + 0x70u);
        aspect = S4_MF(s4_view(0) + 0x74u);
    }
    w = S4_M32(mgr + 0x0u);
    h = S4_M32(mgr + 0x4u);
    for (k = 0; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t v = s4_view(k), x = (k & 1u) ? w / 2u : 0u, y = (k & 2u) ? h / 2u : 0u;
        uint32_t qw = (k & 1u) ? w - w / 2u : w / 2u, qh = (k & 2u) ? h - h / 2u : h / 2u;
        uint32_t a = g_eax, c = g_ecx, d = g_edx;
        sp = g_esp;
        g_esp -= 0x20u;
        rect = g_esp + 0x10u;
        S4_W32(rect + 0x0u) = x;
        S4_W32(rect + 0x4u) = y;
        S4_W32(rect + 0x8u) = qw;
        S4_W32(rect + 0xCu) = qh;
        PUSH32(g_esp, rect);
        PUSH32(g_esp, S4_M32(mgr + 0x370u));
        PUSH32(g_esp, S4_M32(v + 0x60u));
        PUSH32(g_esp, 0);
        sub_001DE810();
        if (S4_M32(mgr + 0x374u)) {
            g_esp = sp - 0x20u;
            PUSH32(g_esp, rect);
            PUSH32(g_esp, S4_M32(mgr + 0x374u));
            PUSH32(g_esp, S4_M32(v + 0x64u));
            PUSH32(g_esp, 0);
            sub_001DE810();
        }
        g_eax = a; g_ecx = c; g_edx = d; g_esp = sp;
        S4_W32(v + 0x78u) = qw;
        S4_W32(v + 0x7Cu) = qh;
        S4_W32(v + 0x80u) = S4_M32(mgr + 0x390u);
        S4_WF(v + 0x74u) = aspect;
        S4_WF(v + 0x70u) = scale;
    }
    S4_W32(mgr + 0x50u) = 2;
    g_esp = sp0 + 4u;                        /* the title's ret: its return address */
    fprintf(stderr, "[SPLIT4] four quarters of %ux%u\n", w, h);
}

/* A guest virtual call without stack arguments: ecx the object, the
 * function at its vtable + off. Registers and esp as they were. */
static void s4_vcall(uint32_t obj, uint32_t off)
{
    uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp;
    recomp_func_t fn = recomp_lookup(S4_M32(S4_M32(obj) + off));
    if (fn) {
        g_ecx = obj;
        PUSH32(g_esp, 0);                       /* its return address */
        fn();
    }
    g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
}

/* A player camera's view link (sub_001674B0's end): the camera's
 * sub-object at + 0x250 and the view point at each other (view + 0x68,
 * sub-object + 0x40), then the sub-object's vtable + 8 sets the view's
 * RwCamera up from it. */
static void s4_link(uint32_t cam, unsigned view)
{
    uint32_t sub = cam + 0x250u, v = s4_view(view);
    S4_W32(v + 0x68u) = sub;
    S4_W32(sub + 0x40u) = v;
    s4_vcall(sub, 8u);
}

/* sub_001674B0 (esi a player camera): binds the camera to its view; the
 * title knows two (player 0 to view 0, any other to view 1, which it then
 * sets up from that camera). Players 3 and 4 are linked to theirs and set
 * up, and view 1 is given back to player 2's camera. */
void split4_camera_bind(void (*gen)(void))
{
    uint32_t cam = g_esi, k;
    gen();
    if (!split4_enabled() || s4_skip("bind"))
        return;
    k = S4_M32(cam + 0xC30u);
    if (k < 2 || k >= SPLIT4_MAX_PLAYERS)
        return;
    s4_link(cam, k);
    s4_link(S4_CAM + S4_CAM_SIZE, 1);
}

/* sub_001AE2F0 (esi the world): the two-player stage draws views 0 and 1;
 * with four racing, views 2 and 3 are prepared (as sub_00020B30 prepares the
 * first two) and drawn the same way. */
void split4_render_views(void (*gen)(void))
{
    uint32_t world = g_esi;
    unsigned k;
    gen();
    if (!s4_four_racing() || s4_skip("render"))
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t v = s4_view(k), human = SPLIT4_HUMAN + k * SPLIT4_HUMAN_SIZE;
        uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp;
        g_edi = v;
        PUSH32(g_esp, 0);
        sub_001C8A60();
        g_esp = sp;
        S4_WF(v + 0x90u) = 1.0f / S4_MF(v + 0x48u);
        g_eax = human;
        g_ebx = v;
        PUSH32(g_esp, s4_fx(k));
        PUSH32(g_esp, 0);
        sub_00179FD0();
        g_esp = sp;
        g_ebx = k;
        g_esi = world;
        PUSH32(g_esp, world);
        PUSH32(g_esp, 0);
        sub_001AE340();
        g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
    }
}
