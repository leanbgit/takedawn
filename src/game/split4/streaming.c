/*
 * streaming.c - the track's stream windows 3 and 4 (players 3 and 4),
 * their section pools, restart, and the traffic around players 3 and 4
 * (docs/split4.md, streaming and traffic).
 */
#include "split4_internal.h"

extern void sub_0019CF20(void);        /* a stream window's frame (eax the window) */
extern uint32_t xbox_ContiguousAllocEx(uint32_t size, uint32_t alignment,
                                       uint32_t phys_lo, uint32_t phys_hi);

/* The traffic's start (sub_001A28B0, the race world's traffic at the GO):
 * for each human (the count at world + 0x12B7C0) it writes the human's
 * track section and spawn count into the streamer's two-entry arrays
 * (streamer + 0x363A7 and + 0x363A9, one byte a focus: the streamer has two
 * foci, players 1 and 2). Players 3 and 4 wrote past them, over the traffic
 * car count (+ 0x363AB) and its neighbour, and no traffic came. It runs for
 * two. */
static int s_traffic34_set;           /* split4_streamer: players 3's and 4's traffic bytes set */

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

/* Each race's load (world load step 1) and each race's end (window 1's
 * release): windows 2 and 3 as their init leaves them, not started, the
 * reader not held by them, their pools all free again. The results' "race
 * again" loads the next race without the windows' init or load step 1,
 * and a race that ends leaves buffers its windows freed (descriptor 2)
 * that only the per-frame pass (s4_pool2_release) makes 0 again: the next
 * race's window 2 waited for a buffer for ever, holding the reader, and
 * the load waited for it. */
extern void sub_0019C6E0_gen(void);     /* a stream window's init (eax) */

void s4_streams_reset(void)
{
    unsigned k;
    uint32_t lock = S4_M32(S4_WIN_LOCK);
    /* windows 2 and 3 as their init leaves them (state 1, nothing held;
     * it also frees the reader, which only a start of 2 or 3 could hold
     * now) */
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
        s4_gcall_r(sub_0019C6E0_gen, s4_win(k), 0, NULL, NULL);
        s_win_started[k] = 0;
    }
    if (lock != 0xFFFFFFFFu && lock != 2u && lock != 3u)
        S4_W32(S4_WIN_LOCK) = lock;     /* windows 0's and 1's own, as it was */
    s_win1_ready = 0;
    memset(s_pool2_age, 0, sizeof s_pool2_age);
    if (s_pool2_a)
        s4_pool2_reset();
    s_traffic34_set = 0;
    fprintf(stderr, "[SPLIT4] streaming for players 3 and 4 reset\n");
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
    if (ours && !s_win1_ready && S4_M32(w + 0x10u) == 2u) {
        /* a start on a running window: the pause menu's Restart (the
         * world's load runs its start steps again); windows 2 and 3 are
         * started again with it (before this they kept their old sections
         * and players 3 and 4 fell through the track) */
        s4_streams_reset();                 /* as the init leaves them, their pool whole */
        fprintf(stderr, "[SPLIT4] restart: windows 2 and 3 start again\n");
    }
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

/* sub_001AABD0 (esi the world): the stage's reset (the pause menu's
 * Restart) sets windows 0's and 1's state (+ 0x10, to 0x19), and the
 * world then starts them again (sub_0019C740, call after call, which
 * takes a window in state 0x19 back to the grid). Windows 2 and 3 follow
 * window 1: its state, and started again with it (they count as not
 * started, so split4_win_start starts them; before this they kept their
 * old sections and players 3 and 4 fell through the track). */
void split4_win_restart(void (*gen)(void))
{
    uint32_t st = S4_M32(s4_win(1) + 0x10u), k;
    gen();
    if (!split4_enabled() || S4_M32(s4_win(1) + 0x10u) == st)
        return;
    for (k = 2; k < SPLIT4_MAX_PLAYERS; k++)
        if (s_win_started[k]) {
            S4_W32(s4_win(k) + 0x10u) = S4_M32(s4_win(1) + 0x10u);
            s_win_started[k] = 0;
        }
    s_win1_ready = 0;
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
    /* the next race may come without an init or a load step 1 (the
     * results' "race again"): windows 2 and 3 fresh for it now */
    s4_streams_reset();
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

void s4_streaming_start_marks(void)
{
    recomp_patch_on_mark("s4_stream_a", split4_stream_cmp);
    recomp_patch_on_mark("s4_stream_b", split4_stream_cmp);
    recomp_patch_on_mark("s4_stream_end", split4_stream_end);
    recomp_patch_on_mark("s4_view_other_a", split4_view_other_a);
    recomp_patch_on_mark("s4_view_other_b", split4_view_other_b);
}
