/*
 * car_select.c - players 3's and 4's turns at the car select
 * (docs/split4.md, "Car select").
 */
#include "split4_internal.h"

/* ---- The car select: turns for players 3 and 4 ----
 *
 * The car select's event method (sub_00066490, the screen's vtable
 * 0x3AC1B8; ret 16: the event, its button, the pad's port, one more)
 * keeps whose turn it is at 0x463B20 and the players who choose at
 * 0x463B14, and each player's pick in the front end's game set-up
 * (0x463AF0: + 0x80 cars, 8 bytes each, + 0xC0 paints, 4 each; slots 2 on
 * are the rivals'). The screen's car is at 0x559648 (+ 8 its id, + 0x28
 * its paint). An A (event 5, button 0) stores the screen's car in the
 * turn's slot; in the two-player race (set-up + 0xC not 4, the front end's
 * multiplayer mode 2 at + 4) player 1's moves the turn to 1 and shows
 * player 2's car, player 2's takes their pad (0x4AED46), fills the rivals
 * by the two cars' classes and starts the race. B (button 3) goes a turn
 * back, or off the screen from player 1's.
 *
 * With four racing, player 2's A opens players 3's and 4's turns instead
 * (the game's turn stays 1: what it keeps per turn is two wide), titled
 * "P3 SELECT CAR", "P4 ..." (the title is "%1 SELECT CAR" with the name in
 * setup record 1, "P2", which reads P3 or P4 for the while; the title is
 * built again with each car shown), and player 4's A
 * gives the game player 2's A, with player 2's car and pad as they were.
 * Their picks go to split4_choose_car. B steps back through the turns. */
#define S4_CS_TURN       0x00463B20u
#define S4_CS_SETUP      0x00463AF0u
#define S4_CS_CAR        0x00559648u   /* the screen's car: + 8 id, + 0x28 paint */

extern void sub_00066AC0(void);        /* show a car (id lo, hi, paint; ret 12) */

static unsigned s_cs_turn;             /* 2 or 3: player 3's or 4's turn; 0 the game's */
static uint32_t s_cs_p2[4];            /* player 2's pick and port: lo, hi, paint, port */
static uint32_t s_cs_title_saved;      /* setup record 1's name ("P2"), while it reads P3 or P4 */
static uint32_t s_cs_pick[SPLIT4_MAX_PLAYERS][3];

/* the title for the game's turn 1 ("%1 SELECT CAR", %1 the name in the
 * setup record of turn & 1, + 4: "P2") names player k (1-based 2, 3, 4) */
static void s4_cs_title(unsigned k)
{
    uint32_t name = S4_SETUP + S4_SETUP_SIZE + 4u;
    if (k == 2u) {
        if (s_cs_title_saved) {
            S4_W32(name) = s_cs_title_saved;
            S4_W16(name + 4u) = 0;
            s_cs_title_saved = 0;
        }
        return;
    }
    if (!s_cs_title_saved)
        s_cs_title_saved = S4_M32(name);
    S4_W16(name) = 'P';
    S4_W16(name + 2u) = (uint16_t)('0' + k);
    S4_W16(name + 4u) = 0;
}

static void s4_cs_show(uint32_t lo, uint32_t hi, uint32_t paint)
{
    uint32_t a[] = { paint, hi, lo };
    s4_gcall(sub_00066AC0, g_eax, g_ecx, g_edx, g_ebx, g_esi, g_edi, 3, a);
}

static void s4_cs_reset(void)
{
    s_cs_turn = 0;
    s4_cs_title(2);
}

void split4_carsel_event(void (*gen)(void))
{
    uint32_t sp = g_esp, ev = S4_M32(sp + 4u), button = S4_M32(sp + 12u), port = S4_M32(sp + 16u);
    uint32_t lo = S4_M32(S4_CS_CAR + 8u), hi = S4_M32(S4_CS_CAR + 12u), paint = S4_M32(S4_CS_CAR + 0x28u);
    int race2 = split4_enabled() && !s4_skip("carsel") && S4_M32(S4_CS_SETUP + 4u) == 2u
             && S4_M32(S4_CS_SETUP + 0xCu) != 4u && S4_M32(S4_CS_TURN) == 1u;

    if (ev == 2u || ev == 3u) {                /* the screen left or entered */
        s4_cs_reset();
        gen();
        return;
    }
    if (ev != 5u || !race2 || (button != 0u && button != 3u)) {
        gen();
        return;
    }
    if (button == 3u) {                        /* B */
        if (s_cs_turn == 0) {
            gen();
            return;
        }
        if (s_cs_turn == 2u) {                 /* back to player 2's */
            s4_cs_reset();
            s4_cs_show(s_cs_p2[0], s_cs_p2[1], s_cs_p2[2]);
        } else {                               /* back to player 3's */
            s_cs_turn = 2;
            s4_cs_title(3);
            s4_cs_show(s_cs_pick[2][0], s_cs_pick[2][1], s_cs_pick[2][2]);
        }
        g_esp += 20u;                          /* its ret 16 */
        return;
    }
    if (s_cs_turn == 0) {                      /* player 2's A: player 3's turn */
        s_cs_p2[0] = lo; s_cs_p2[1] = hi; s_cs_p2[2] = paint; s_cs_p2[3] = port;
        S4_W32(S4_CS_SETUP + 0x88u) = lo;      /* as the game stores it */
        S4_W32(S4_CS_SETUP + 0x8Cu) = hi;
        S4_W32(S4_CS_SETUP + 0xC4u) = paint;
        s_cs_turn = 2;
        s4_cs_title(3);
        if (!s_cs_pick[2][0] && !s_cs_pick[2][1]) {
            s_cs_pick[2][0] = lo; s_cs_pick[2][1] = hi; s_cs_pick[2][2] = paint;
        }
        s4_cs_show(s_cs_pick[2][0], s_cs_pick[2][1], s_cs_pick[2][2]);
        g_esp += 20u;
        return;
    }
    s_cs_pick[s_cs_turn][0] = lo; s_cs_pick[s_cs_turn][1] = hi; s_cs_pick[s_cs_turn][2] = paint;
    split4_choose_car(s_cs_turn, lo, hi, paint);
    {
        char name[16];
        s4_car_name(lo, hi, name);
        fprintf(stderr, "[SPLIT4] car select: player %u picks %s, paint %u\n", s_cs_turn + 1, name, paint);
    }
    if (s_cs_turn == 2u) {                     /* player 3's A: player 4's turn */
        s_cs_turn = 3;
        s4_cs_title(4);
        if (!s_cs_pick[3][0] && !s_cs_pick[3][1]) {
            s_cs_pick[3][0] = lo; s_cs_pick[3][1] = hi; s_cs_pick[3][2] = paint;
        }
        s4_cs_show(s_cs_pick[3][0], s_cs_pick[3][1], s_cs_pick[3][2]);
        g_esp += 20u;
        return;
    }
    /* player 4's A: the game's player 2 A, as player 2 gave it */
    s4_cs_reset();
    S4_W32(S4_CS_CAR + 8u) = s_cs_p2[0];
    S4_W32(S4_CS_CAR + 12u) = s_cs_p2[1];
    S4_W32(S4_CS_CAR + 0x28u) = s_cs_p2[2];
    S4_W32(sp + 16u) = s_cs_p2[3];
    gen();
}
