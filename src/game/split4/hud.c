/*
 * hud.c - players 3's and 4's HUD: a second HUD manager, the four
 * panels' layout and clip frames (docs/split4.md, the HUD).
 */
#include "split4_internal.h"

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
    /* the award sign (sub_00054630's, P1's 0x3FE268) keeps the player it
     * was made for at + 0x20 (its human at + 0x24 comes from it) and later
     * takes its panel by it (manager + index x 0x70: sub_00054700,
     * sub_00054B50): players 3's and 4's read past S4_HUD2's two panels and
     * never showed a sign */
    S4_W32(s4_w(w, 0x003FE268u) + 0x20u) = k - 2u;
}

/* sub_00052AB0 (ebx the manager): the race's HUD build; for the two-player
 * race, P1's and P2's widgets in 0x54F900's panels. With four racing,
 * players 3's and 4's in S4_HUD2's after it. */
/* A panel's clip frame (the record's + 0x64): the build gives each panel
 * one (sub_00053AA0: eax the record, esi its size, its position on the
 * stack, ret 4; the two-player build's halves, the frame's width by
 * 240), and each widget's layers clip to it (sub_0004E360, at the
 * widget's creation). Drawn through it are the boost bar and its flame
 * and the award messages; the texts and plates are not, which is how
 * player 2's bar and messages went (its frame was still the bottom
 * half while its panel was the top-right quarter). The frame's fields:
 * position + 0, + 4, size + 8, + 0xC (the scene's frame tree scales them
 * into its world fields at the draw). */
extern void sub_00053AA0(void);

static void s4_hud_clip_new(uint32_t r, float x, float y, float w, float h)
{
    uint32_t sp = g_esp, pos, size;
    g_esp -= 0x20u;
    pos = g_esp + 0x10u;
    size = g_esp + 0x18u;
    S4_WF(pos) = x;
    S4_WF(pos + 4u) = y;
    S4_WF(size) = w;
    S4_WF(size + 4u) = h;
    s4_gcall(sub_00053AA0, r, 0, 0, 0, size, r, 1, &pos);
    g_esp = sp;
}

static void s4_hud_clip_set(uint32_t r, float x, float y, float w, float h)
{
    uint32_t f = S4_M32(r + 0x64u);
    if (!f)
        return;
    S4_WF(f) = x;
    S4_WF(f + 4u) = y;
    S4_WF(f + 8u) = w;
    S4_WF(f + 0xCu) = h;
}

void split4_hud_build(void (*gen)(void))
{
    uint32_t m = g_ebx, k, r, n, sv, f;
    float cx = 0.0f, cw = 640.0f, ch = 240.0f;
    gen();
    if (m != S4_HUD || !s4_four_racing() || S4_M32(m + 0x170u) != 2u)
        return;
    /* players 1's and 2's clip frames: the top quarters. The build's top
     * half gives the picture's span in the HUD's units: x 0 and width 640
     * in 4:3; with display.hud_fix (the patches in sub_00052AB0) the
     * wider picture's, x (640 - W)/2 and width W = 480 x aspect, which the
     * scene's squeeze brings back to the screen's edges. */
    f = S4_M32(m + 0x64u);
    if (f && S4_MF(f + 8u) > 0.0f && S4_MF(f + 0xCu) > 0.0f) {
        cx = S4_MF(f);
        cw = S4_MF(f + 8u);
        ch = S4_MF(f + 0xCu);
    }
    if (!s4_skip("clip")) {
        s4_hud_clip_set(m, cx, 0.0f, cw * 0.5f, ch);
        s4_hud_clip_set(m + S4_HUD_REC, cx + cw * 0.5f, 0.0f, cw * 0.5f, ch);
    }
    if (!s_hud2 || s4_skip("hud2"))
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
        /* players 3's and 4's clip frames, the bottom quarters, before
         * their widgets are made (they take it then) */
        if (!s4_skip("clip"))
            s4_hud_clip_new(r, cx + (k ? cw * 0.5f : 0.0f), ch, cw * 0.5f, ch);
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
    if (n != 2u || m != S4_HUD || !s4_four_racing() || s4_skip("layout"))
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
