/*
 * cars.c - players 3's and 4's cars in the race (docs/split4.md, "Cars").
 */
#include "split4_internal.h"

#include <time.h>

extern void sub_00015900(void);        /* the player camera's constructor (ecx) */
extern void sub_001673E0(void);        /* the player camera's init (ecx; eax the player) */

/* ---- Cars for players 3 and 4 ----
 *
 * The front end hands the race its cars (sub_0005F7B0): the module's six
 * car slots (+ 0x10, 8 bytes each: a car's id, its name packed base 40,
 * "COMPCAR3") with a paint each (+ 0x78, 4 bytes each), players 1 and 2
 * first, then the rivals. The stage's start (sub_00017450) gives human i
 * slot i (human + 0x1970 the id, + 0x1980 its class from the title's car
 * table, + 0x69 the paint) and the AI rivals slots 2, 3, ... whatever the
 * human count (+ 0x20 on, AI racers 0x73F190 + 0x2470 x j). So players 3
 * and 4 took the first two rivals' cars, and the two AIs drove the same.
 *
 * A player's choice (split4_choose_car: a menu turn later; until then
 * BO3_SPLIT4_CARS) goes in slots 2 and 3 for the start, and the two AI
 * rivals get the rivals' cars those slots held right after it.
 * BO3_SPLIT4_CARS: "copy" (the default: player 3 player 1's car, player 4
 * player 2's), "random" (any of the 50 racing cars), or two names,
 * "SPRTCAR2,MSCLCAR5" (a name may end in ":paint", "COMPCAR4:2"; one value
 * alone is for both). Each takes the paint of the player it copies (3:
 * player 1's, 4: player 2's) unless given. */
#define S4_CAR_SLOTS     0x10u         /* module: six cars, 8 bytes each */
#define S4_CAR_PAINTS    0x78u         /* module: six paints, 4 bytes each */
#define S4_CAR_TABLE     0x0039FC90u   /* the title's 67 cars (ids) */
#define S4_CAR_CLASSES   0x0039FEA8u   /* their classes */
#define S4_CAR_COUNT     67u
#define S4_CAR_RACING    50u           /* COMP .. SUPR, ten each */
#define S4_AI_RACER      0x0073F190u   /* the AI racers, 0x2470 each */
#define S4_AI_SIZE       0x2470u

static const char s4_b40[] = " -/0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_";

static struct { uint32_t lo, hi, paint; int set; } s_car_choice[SPLIT4_MAX_PLAYERS];

/* For a front-end turn later: player k's (2 or 3) car for the next race. */
void split4_choose_car(unsigned k, uint32_t id_lo, uint32_t id_hi, uint32_t paint)
{
    if (k < SPLIT4_MAX_PLAYERS) {
        s_car_choice[k].lo = id_lo;
        s_car_choice[k].hi = id_hi;
        s_car_choice[k].paint = paint;
        s_car_choice[k].set = 1;
    }
}

static int s4_car_index(uint32_t lo, uint32_t hi)
{
    unsigned i;
    for (i = 0; i < S4_CAR_COUNT; i++)
        if (S4_M32(S4_CAR_TABLE + i * 8u) == lo && S4_M32(S4_CAR_TABLE + i * 8u + 4u) == hi)
            return (int)i;
    return -1;
}

void s4_car_name(uint32_t lo, uint32_t hi, char *out)
{
    uint64_t x = ((uint64_t)hi << 32) | lo;
    char t[13];
    int n = 0, i;
    while (x && n < 12) {
        t[n++] = s4_b40[x % 40u];
        x /= 40u;
    }
    for (i = 0; i < n; i++)
        out[i] = t[n - 1 - i];
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = 0;
}

/* "COMPCAR3" -> its id; 0 for a name the alphabet cannot hold */
static int s4_car_id(const char *name, size_t len, uint32_t *lo, uint32_t *hi)
{
    uint64_t x = 0;
    size_t i;
    if (len == 0 || len > 12)
        return 0;
    for (i = 0; i < 12; i++) {
        char c = i < len ? name[i] : ' ';
        const char *p;
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        p = strchr(s4_b40, c);
        if (!p || !c)
            return 0;
        x = x * 40u + (uint64_t)(p - s4_b40);
    }
    *lo = (uint32_t)x;
    *hi = (uint32_t)(x >> 32);
    return 1;
}

/* player k's (2 or 3) car and paint for this race: the menu's, else the
 * setting's */
static void s4_car_pick(uint32_t mod, unsigned k, uint32_t *lo, uint32_t *hi, uint32_t *paint,
                        const char **how)
{
    unsigned from = k - 2u;                  /* the player copied: 1 or 2 */
    const char *v = getenv("BO3_SPLIT4_CARS"), *w;
    size_t len = 0;
    unsigned i;

    *lo = S4_M32(mod + S4_CAR_SLOTS + from * 8u);
    *hi = S4_M32(mod + S4_CAR_SLOTS + from * 8u + 4u);
    *paint = S4_M32(mod + S4_CAR_PAINTS + from * 4u);
    *how = "copy";
    if (s_car_choice[k].set) {
        *lo = s_car_choice[k].lo;
        *hi = s_car_choice[k].hi;
        *paint = s_car_choice[k].paint;
        *how = "chosen";
        return;
    }
    /* this player's part of the setting: the (k-2)th comma field, or the
     * only one */
    w = v;
    if (w && *w) {
        for (i = 0; i < from && strchr(w, ','); i++)
            w = strchr(w, ',') + 1;
        len = strcspn(w, ",");
    }
    if (!w || !len || (len == 4 && !strncmp(w, "copy", 4)))
        return;
    if (len == 6 && !strncmp(w, "random", 6)) {
        static int seeded;
        unsigned r;
        if (!seeded++)
            srand((unsigned)time(NULL));
        r = (unsigned)rand() % S4_CAR_RACING;
        *lo = S4_M32(S4_CAR_TABLE + r * 8u);
        *hi = S4_M32(S4_CAR_TABLE + r * 8u + 4u);
        *how = "random";
        return;
    }
    {
        size_t nl = strcspn(w, ",:");
        uint32_t l, h;
        if (nl > len)
            nl = len;
        if (!s4_car_id(w, nl, &l, &h) || s4_car_index(l, h) < 0) {
            fprintf(stderr, "[SPLIT4] BO3_SPLIT4_CARS: no car \"%.*s\" (COMPCAR1 .. SUPRCAR10, "
                            "HSPCCAR20 .. HSPCCAR25, HEVYCAR1 .. HEVYCAR10, HEVYCAR36); player %u copies\n",
                    (int)nl, w, k + 1);
            return;
        }
        *lo = l;
        *hi = h;
        if (nl < len && w[nl] == ':')
            *paint = (uint32_t)strtoul(w + nl + 1, NULL, 10);
        *how = "set";
    }
}

/* a racer's car: id, class, paint (as the stage's start writes them) */
static void s4_racer_car(uint32_t racer, uint32_t lo, uint32_t hi, uint32_t paint)
{
    int i = s4_car_index(lo, hi);
    S4_W32(racer + 0x1970u) = lo;
    S4_W32(racer + 0x1974u) = hi;
    S4_W32(racer + 0x1980u) = i >= 0 ? S4_M32(S4_CAR_CLASSES + (uint32_t)i * 4u) : 0;
    S4_W8(racer + 0x69u) = (uint8_t)paint;
}

/* The stage's start (sub_00017450, ecx the module): four humans, the AIs to
 * fill six cars, players 3's and 4's cars (above), and cameras 2 and 3
 * built once (the app's constructor builds two, 0x157D3). */
void split4_stage_start(void (*gen)(void))
{
    uint32_t mod = g_ecx;
    unsigned k;
    uint32_t rival[4], rpaint[2];
    int cars = 0;

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
        if (!s4_skip("cars")) {
            /* slots 2 and 3: players 3's and 4's for the start, the
             * rivals' kept for the AIs */
            for (k = 0; k < 4u; k++)
                rival[k] = S4_M32(mod + S4_CAR_SLOTS + 0x10u + k * 4u);
            rpaint[0] = S4_M32(mod + S4_CAR_PAINTS + 8u);
            rpaint[1] = S4_M32(mod + S4_CAR_PAINTS + 12u);
            for (k = 2; k < SPLIT4_MAX_PLAYERS; k++) {
                uint32_t lo, hi, paint;
                const char *how;
                char name[16];
                s4_car_pick(mod, k, &lo, &hi, &paint, &how);
                S4_W32(mod + S4_CAR_SLOTS + k * 8u) = lo;
                S4_W32(mod + S4_CAR_SLOTS + k * 8u + 4u) = hi;
                S4_W32(mod + S4_CAR_PAINTS + k * 4u) = paint;
                s4_car_name(lo, hi, name);
                fprintf(stderr, "[SPLIT4] player %u's car: %s, paint %u (%s)\n", k + 1, name, paint, how);
            }
            cars = 1;
        }
    }
    g_ecx = mod;
    gen();
    if (cars) {
        /* the AI rivals (they read slots 2 on) get the rivals' cars */
        for (k = 0; k < 2u && k < S4_M32(mod + S4_STAGE_AIS); k++) {
            char name[16];
            s4_racer_car(S4_AI_RACER + k * S4_AI_SIZE, rival[k * 2u], rival[k * 2u + 1u], rpaint[k]);
            s4_car_name(rival[k * 2u], rival[k * 2u + 1u], name);
            fprintf(stderr, "[SPLIT4] rival %u's car: %s, paint %u\n", k + 1, name, rpaint[k]);
        }
        for (k = 0; k < 4u; k++)
            S4_W32(mod + S4_CAR_SLOTS + 0x10u + k * 4u) = rival[k];
        S4_W32(mod + S4_CAR_PAINTS + 8u) = rpaint[0];
        S4_W32(mod + S4_CAR_PAINTS + 12u) = rpaint[1];
    }
}

/* ---- The AI racers' look at the humans ----
 *
 * An AI racer's driver (0x1580 bytes, vtable 0x3B1240; world + 0x26120)
 * keeps a squared distance to each human at + 0x1560, two floats; its own
 * car is at + 0x1568 (sub_001049A0 sets it). Its update (sub_00104A90)
 * first looks at the humans (sub_00105BD0): for each of the 0x73A1C0 humans
 * the distance goes to the next float, through a pointer that starts at
 * + 0x1560 (two loops, one for a human car the AI drives, 0x105C83, one for
 * a rival, 0x105DD3: `mov edx, [esp + ...]`, then `movss [edx], xmm4`).
 * With four humans players 3's and 4's distances went over the car pointer
 * and the float after it; the function, and sub_00106370 after it, then
 * read the car through a float (the crash at sub_00106370 + 0x9C, at a
 * race's start or later, wherever that float pointed). Only sub_00106370
 * reads the floats, players 1's and 2's (by index & 1), so players 3's
 * and 4's go to two spare floats instead (marks s4_ai_near_a, _b, after
 * the load of the pointer). */
#define S4_AI_NEAR       0x007BEB00u   /* players 3's and 4's distances, unread */
#define S4_AI_DIST       0x1560u

static void split4_ai_near(void)
{
    uint32_t off = g_edx - g_esi;
    if (off >= S4_AI_DIST + 8u && off < S4_AI_DIST + 16u)
        g_edx = S4_AI_NEAR + (off - S4_AI_DIST - 8u);
}

void s4_cars_start_marks(void)
{
    recomp_patch_on_mark("s4_ai_near_a", split4_ai_near);
    recomp_patch_on_mark("s4_ai_near_b", split4_ai_near);
}
