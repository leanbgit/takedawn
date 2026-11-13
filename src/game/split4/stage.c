/*
 * stage.c - the four-player stage: four humans on the two-player stage,
 * their setup records and pads, the arrow keys, the AI for players 3 and 4
 * (docs/split4.md items 1 to 3).
 */
#include "split4_internal.h"

extern void sub_001B4230(void);        /* the player camera's reset, vtable +0xC (ecx; mode, 0) */
extern void sub_00179760(void);        /* the AI driver's route reset (eax) */

/* ---- The four-player stage (step 3, bring-up) --------------------------
 *
 * BO3_SPLIT4=1 turns the title's two-player stage into a four-player one:
 * four human racers (and at most two AI rivals, six cars in all), setup
 * records and cameras for players 3 and 4. Without it nothing here acts and
 * the two-player stage is the title's. Players 3 and 4 drive with the
 * title's pad ports 0 and 1 (RECOMP_USB_PADS=4: the third and fourth
 * controllers); BO3_SPLIT4_AI has the title's AI drive either or both
 * instead (s4_ai34). */

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
    s4_streams_reset();
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
    if (!split4_enabled() || !s4_stage_is_2p())
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
            who = !(s4_ai34() & 1) ? 2 : !(s4_ai34() & 2) ? 3 : -2;
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

/* The car update mark (ap_update, sub_0018C510, esi the car): with
 * BO3_SPLIT4_AI the title's AI takes players 3 and 4 once they are in play,
 * as autopilot.c does for player 1. */
static void split4_car_update(void)
{
    uint32_t car = g_esi;

    if (!((car == SPLIT4_HUMAN + 2 * SPLIT4_HUMAN_SIZE && (s4_ai34() & 1)) ||
          (car == SPLIT4_HUMAN + 3 * SPLIT4_HUMAN_SIZE && (s4_ai34() & 2))))
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

void s4_stage_start_marks(void)
{
    if (s4_ai34() && !recomp_env_on(RENV_AUTOPILOT))
        recomp_patch_on_mark("ap_update", split4_car_update);
}
