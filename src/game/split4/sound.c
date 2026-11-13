/*
 * sound.c - players 3's and 4's engines and listeners (docs/split4.md
 * item 7).
 */
#include "split4_internal.h"

/* ---- Sound: players 3 and 4 ----
 *
 * The race audio (manager 0x40E120) builds a listener for every human from
 * that player's camera (sub_0014C880, sound object 0x40F270 + 0x250, 0x40
 * each; with four the last two cover a file name only the load uses), and
 * the checks for "is this sound near a player" loop over every human: those
 * take four as they are. Two places are made for two:
 *
 * - The car loop (sub_0013B270) keeps one float per player at 0x40E310
 *   (the engine block 0x40E130 + 0x1E0, indexed by the human's player
 *   number) and sub_00155FD0 reads it back. The bytes after the two are a
 *   table the engine sound reads (0x40E318): players 3 and 4 use S4_SND_F
 *   instead (marks s4_snd_set, s4_snd_get, s4_snd_get_end).
 * - A car's engine sound (racer + 0x1924) comes from two "player engine"
 *   objects (vtable 0x3AD130, engine block + 0x210 and + 0x3B0, 0x1A0
 *   each, in use at + 0xC5: sub_0018C6F0, the human racer's), heard as the
 *   driver hears his own car (mixbins 17 and 18, the GP's distortion), and
 *   when both are taken from the rivals' five (vtable 0x3AD15C, + 0x550,
 *   0x168 each), heard from outside, placed in the world. Players 3 and 4
 *   get two more player engines here, built as the audio manager's
 *   constructor builds them (sub_00134FE0: the vtable and nothing else; the
 *   engine's own attach, vtable + 0, sets the rest up). */
#define S4_SND_F         0x007BE000u   /* players 3's and 4's floats */
#define S4_ENG34         0x007BE100u   /* players 3's and 4's player engines */
#define S4_ENG_SIZE      0x000001A0u
#define S4_ENG_VT        0x003AD130u
#define S4_SND_F_OLD     0x0040E310u   /* 0x40E130 + 0x1E0 */

static void split4_snd_set(void)
{
    if (g_edx >= 2u && g_edx < SPLIT4_MAX_PLAYERS)
        g_edx = (S4_SND_F + (g_edx - 2u) * 4u - S4_SND_F_OLD) / 4u;
}

static uint32_t s_snd_ebx = 0xFFFFFFFFu;

static void split4_snd_get(void)
{
    s_snd_ebx = 0xFFFFFFFFu;
    if (g_ebx >= 2u && g_ebx < SPLIT4_MAX_PLAYERS) {
        s_snd_ebx = g_ebx;
        g_ebx = (S4_SND_F + (g_ebx - 2u) * 4u - S4_SND_F_OLD) / 4u;
    }
}

static void split4_snd_get_end(void)
{
    if (s_snd_ebx != 0xFFFFFFFFu)
        g_ebx = s_snd_ebx;
    s_snd_ebx = 0xFFFFFFFFu;
}

/* sub_0018C6F0 (ecx the human racer, ret; al back): the engine sound of a
 * human's car, see above. Players 3 and 4 take theirs (player 3 the first,
 * player 4 the second) when it is free; otherwise as the title does. */
void split4_engine_get(void (*gen)(void))
{
    uint32_t r = g_ecx, k, e, st = S4_M32(0x004D5370u);
    if (!s4_four_racing() || s4_skip("engine34") || S4_M32(r + 0x1924u)
        || r < SPLIT4_HUMAN + 2u * SPLIT4_HUMAN_SIZE
        || r >= SPLIT4_HUMAN + SPLIT4_MAX_PLAYERS * SPLIT4_HUMAN_SIZE
        || (r - SPLIT4_HUMAN) % SPLIT4_HUMAN_SIZE || !st
        || st == 0x004D4290u || st == 0x004D4008u) {
        gen();
        return;
    }
    k = (r - SPLIT4_HUMAN) / SPLIT4_HUMAN_SIZE;
    e = S4_ENG34 + (k - 2u) * S4_ENG_SIZE;
    if (S4_M32(e) != S4_ENG_VT) {
        memset((void *)((uintptr_t)g_xbox_mem_offset + e), 0, S4_ENG_SIZE);
        S4_W32(e) = S4_ENG_VT;
    }
    if (S4_M8(e + 0xC5u)) {
        /* still taken from the last race: each is player k's alone, and a
         * new race's racer starts with no engine (sub_0018D020) without
         * giving it back, so it is let go here (vtable + 8, as
         * sub_0018DD20 does) */
        s4_vcall(e, 8u);
        S4_W8(e + 0xC5u) = 0;
    }
    S4_W8(e + 0xC5u) = 1;
    /* + 0xC6: the player's own engine (set on the title's two, 0 on the
     * rivals'): the attach picks the engine's tuning by it (sub_001371A0),
     * the car loop its per-player float (sub_0013B270) */
    S4_W8(e + 0xC6u) = 1;
    S4_W32(r + 0x1924u) = e;
    {
        uint32_t a = g_eax, b = g_ebx, c = g_ecx, d = g_edx, si = g_esi, di = g_edi, sp = g_esp, res = 1;
        recomp_func_t fn = recomp_lookup(S4_M32(S4_ENG_VT));
        if (fn) {
            PUSH32(g_esp, (uint32_t)(int32_t)(int8_t)S4_M8(r + 0x19BCu));
            PUSH32(g_esp, S4_M32(r + 0x1974u));
            PUSH32(g_esp, S4_M32(r + 0x1970u));
            PUSH32(g_esp, 0);
            g_ecx = e;
            fn();
            res = g_eax;
        }
        g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
        g_eax = (a & ~0xFFu) | (res & 0xFFu);
    }
    fprintf(stderr, "[SPLIT4] player %u's engine sound: a player's (0x%08X)\n", k + 1u, e);
    g_esp += 4;                                 /* ret */
}

/* ---- Sound: listeners for players 3 and 4 ----
 *
 * The positioned sounds (the 0x40B844 emitter system: the other cars'
 * engines, players 2 to 4's own included, traffic, impacts) are placed and
 * attenuated from the nearer of the listeners the audio world (0x40B310)
 * holds at + 0x520 (two pointers, the count at + 0x528; sub_001CA270: ecx
 * the point, edx the holder, eax the nearer): two listener objects
 * (0x411E80, 0x411E9C, 0x1C each: vtable 0x3AD188, + 8 a DirectSound 3D
 * object the title makes with sub_001F7150 (0, 4, 0, 0) and sub_001F77C0,
 * + 0x10 the player's camera, + 0x14 the player; sub_0013FA60 (vtable + 4,
 * the index on the stack) binds one to a player, sub_0013FB30 (vtable + 0)
 * updates it from the camera each frame, from the audio world's update
 * sub_001CA530). With four, players 3's and 4's engines were heard from
 * player 1's or 2's camera, far away: two more listener objects here,
 * made as the title's are, bound to players 3 and 4, updated after the
 * title's (mark s4_listeners), and in sub_001CA270's choice. */
#define S4_LST           0x007BE900u   /* two listener objects, 0x20 apart */
#define S4_LST_VT        0x003AD188u
#define S4_AUDIO_WORLD   0x0040B310u

extern void sub_001F7150(void), sub_001F77C0(void), sub_0013FA60(void);

static int s_lst_built;

static void s4_listeners_build(void)
{
    unsigned k;
    if (s_lst_built || !s4_four_racing())
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t o = S4_LST + (k - 2u) * 0x20u, h = 0, a[] = { 0, 0, 4, 0 };
        s4_gcall_r(sub_001F7150, 0, 4, a, &h);
        if (!h) {
            fprintf(stderr, "[SPLIT4] no 3D listener object for player %u\n", k + 1u);
            return;
        }
        s4_gcall_r(sub_001F77C0, 0, 1, &h, NULL);
        memset((void *)((uintptr_t)g_xbox_mem_offset + o), 0, 0x20u);
        S4_W32(o) = S4_LST_VT;
        S4_W32(o + 8u) = h;
        {
            uint32_t idx = k;
            uint32_t a2[] = { idx };
            s4_gcall(sub_0013FA60, 0, o, 0, 0, 0, 0, 1, a2);   /* bind: ecx the object, ret 4 */
        }
        fprintf(stderr, "[SPLIT4] listener for player %u (0x%08X, camera 0x%08X)\n",
                k + 1u, o, S4_M32(o + 0x10u));
    }
    s_lst_built = 1;
}

/* Mark s4_listeners (sub_001CA530, after the loop that updates the
 * title's listeners; edi the audio world): ours updated too. */
static void split4_listeners(void)
{
    unsigned k;
    if (!s4_four_racing() || s4_skip("listen34"))
        return;
    s4_listeners_build();
    if (!s_lst_built)
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++)
        s4_vcall(S4_LST + (k - 2u) * 0x20u, 0);
}

/* sub_001CA270 (ecx the point, edx the holder; eax the nearer listener):
 * ours in the choice. */
void split4_nearest_listener(void (*gen)(void))
{
    uint32_t pt = g_ecx, best, k;
    float px, py, pz, bd;
    int use = s_lst_built && s4_four_racing() && !s4_skip("listen34");
    if (use) {
        px = S4_MF(pt); py = S4_MF(pt + 4u); pz = S4_MF(pt + 8u);
    }
    gen();
    if (!use)
        return;
    best = g_eax;
    {
        uint32_t h = S4_M32(best + 8u);
        float dx = S4_MF(h + 0x30u) - px, dy = S4_MF(h + 0x34u) - py, dz = S4_MF(h + 0x38u) - pz;
        bd = dx * dx + dy * dy + dz * dz;
    }
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t o = S4_LST + (k - 2u) * 0x20u, h = S4_M32(o + 8u);
        float dx, dy, dz, d;
        if (!h)
            continue;
        dx = S4_MF(h + 0x30u) - px; dy = S4_MF(h + 0x34u) - py; dz = S4_MF(h + 0x38u) - pz;
        d = dx * dx + dy * dy + dz * dz;
        if (d < bd) {
            bd = d;
            best = o;
        }
    }
    g_eax = best;
}

void s4_sound_start_marks(void)
{
    recomp_patch_on_mark("s4_snd_set", split4_snd_set);
    recomp_patch_on_mark("s4_listeners", split4_listeners);
    recomp_patch_on_mark("s4_snd_get", split4_snd_get);
    recomp_patch_on_mark("s4_snd_get_end", split4_snd_get_end);
}
