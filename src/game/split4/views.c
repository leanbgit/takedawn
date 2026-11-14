/*
 * views.c - four views, one per quarter: players 3's and 4's views and
 * effects records, the split, their draw and their sky (docs/split4.md,
 * views).
 */
#include "split4_internal.h"

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

#define S4_VIEW34       0x007B3000u   /* views of players 3 and 4, 0xA0 each */
#define S4_FX34         0x007B3200u   /* their effects records, 0xA0 each */
#define S4_VIEW_SIZE    0xA0u

extern void sub_001C8C10(void);        /* a view's set-up (esi the view; one arg, ret 4) */
extern void sub_001D9290(void);        /* RwCameraBeginUpdate (cdecl) */
extern void sub_001DE810(void);        /* RwRasterSubRaster (cdecl: sub, parent, rect) */
extern void sub_001AE340(void);        /* the stage render of one view (ebx the view; world, ret 4) */
extern void sub_001C8A60(void);        /* a view's matrices from its player camera (edi) */
extern void sub_00179FD0(void);        /* a view's effects record (eax the player, ebx the view; cdecl fx) */
extern void sub_0002F380(void);        /* the single-view builder (edi the manager; index, ret 4) */

uint32_t s4_view(unsigned k)
{
    return k < 2 ? S4_MGR + 0x90u + k * S4_VIEW_SIZE : S4_VIEW34 + (k - 2) * S4_VIEW_SIZE;
}

uint32_t s4_fx(unsigned k)
{
    return k < 2 ? S4_MGR + 0x1D0u + k * S4_VIEW_SIZE : S4_FX34 + (k - 2) * S4_VIEW_SIZE;
}

/* s4_env_view: the object at 0x60E040 keeps an entry per view (+ 0xB0,
 * 0x40 each) for two views, its current view (+ 0x130) right after them.
 * Views 2 and 3 take entries 0 and 1, with their own entries set in for
 * their draws (s4_sky_swap). */
static void split4_env_view(void)
{
    uint32_t v = S4_M32(0x0060E170u);
    if (v >= 2u && v < SPLIT4_MAX_PLAYERS)
        S4_W32(0x0060E170u) = v & 1u;
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

/* The sky's frame (sub_00188D70, ebx the sky at 0x60E040; one arg, ret 4):
 * for each human (the count at world + 0x12B7C0) the sky's colour at the
 * human's place on the track (sub_00189940: eax the human, edx the sky) goes
 * into the sky's entry for that view (+ 0xB0, 0x40 each, two of them), and
 * the entry's + 0x20 into the view's effects record (+ 0x90). Players 3 and
 * 4 wrote past the two entries, over the sky's own fields after them (its
 * view, the sun's turn at + 0x134, its state and flags), and the sun of
 * players 1 and 2 moved with players 3 and 4. It runs for two; players 3
 * and 4 get entries of their own (S4_SKY34), set in for their views
 * (split4_render_views). */
#define S4_SKY           0x0060E040u
#define S4_SKY34         0x007BEA00u   /* players 3's and 4's sky entries, 0x40 each */
#define S4_SKY_ENTRY     0x40u
extern void sub_00189940(void);

void split4_sky_update(void (*gen)(void))
{
    uint32_t n = S4_M32(SPLIT4_WORLD + 0x12B7C0u), k;
    int on;

    if (!split4_enabled() || n <= 2u) {
        gen();
        return;
    }
    S4_W32(SPLIT4_WORLD + 0x12B7C0u) = 2u;
    gen();
    S4_W32(SPLIT4_WORLD + 0x12B7C0u) = n;
    on = S4_M8(0x0073A199u) || S4_M8(0x0073A198u);
    for (k = 2; k < SPLIT4_MAX_PLAYERS && k < n; k++) {
        uint32_t e = S4_SKY34 + (k - 2u) * S4_SKY_ENTRY;
        /* the colour is worked out only while the flags ask for it (the
         * race's first frames); one not set yet is worked out now */
        if (on || S4_M32(e + 0x20u) == 0u)
            s4_gcall(sub_00189940, k, 0, e - 0xB0u - k * S4_SKY_ENTRY, g_ebx, 0, 0, 0, NULL);
        S4_W32(s4_fx(k) + 0x90u) = S4_M32(e + 0x20u);
    }
}

/* Views 2 and 3 draw with the sky's entry 0 or 1 (s4_env_view): their own
 * entry in it for the draw, the one it had back after. */
static void s4_sky_swap(unsigned k)
{
    uint8_t *mem = (uint8_t *)(uintptr_t)g_xbox_mem_offset;
    uint8_t *a = mem + S4_SKY + 0xB0u + (k & 1u) * S4_SKY_ENTRY;
    uint8_t *b = mem + S4_SKY34 + (k - 2u) * S4_SKY_ENTRY, t[S4_SKY_ENTRY];
    memcpy(t, a, sizeof t);
    memcpy(a, b, sizeof t);
    memcpy(b, t, sizeof t);
}

static void s4_cube_frame_end(void);

/* sub_001AE2F0 (esi the world): the two-player stage draws views 0 and 1;
 * with four racing, views 2 and 3 are prepared (as sub_00020B30 prepares the
 * first two) and drawn the same way. */
void split4_render_views(void (*gen)(void))
{
    uint32_t world = g_esi;
    unsigned k;
    gen();
    if (!s4_four_racing() || s4_skip("render")) {
        s4_cube_frame_end();
        return;
    }
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
        s4_sky_swap(k);
        sub_001AE340();
        s4_sky_swap(k);
        g_eax = a; g_ebx = b; g_ecx = c; g_edx = d; g_esi = si; g_edi = di; g_esp = sp;
    }
    s4_cube_frame_end();
}

void s4_views_start_marks(void)
{
    recomp_patch_on_mark("s4_env_view", split4_env_view);
}

/* ---- A reflection cube per view, redrawn in turns -----------------------
 *
 * The car's reflection cube (docs/effects.md): the render manager's cube
 * object (S4_CUBE, manager + 0x9D0, made once at boot by sub_00040B90) has
 * the cube texture at + 0xC0 and its six faces (surfaces into the texture's
 * memory) at + 0xC8; sub_0003FA20 (stdcall: the manager, a point, the view)
 * draws the six faces around the view's car at the start of every view's
 * render, and the cars of that view read the texture at + 0xC0
 * (sub_00031690). One cube for all views, so with four views 24 faces a
 * frame: about 1,300 of a four-player frame's 3,500 draws.
 *
 * Views 1 to 3 get cubes of their own (made by the same sub_00040B90, on
 * the scratch objects S4_CUBE_OBJ, right after the title's own at boot),
 * set in at the view's cube call; a view redraws its faces once every
 * s4_cube_every() frames (BO3_SPLIT4_CUBE, default 4: 1 is every frame,
 * as shipped), the views in turn, and in between takes the title's own no-reflection
 * path (the gate words 0x73A190 / 0x73A194 off for the call: the target is
 * put back, the faces stay as last drawn). The title's cube is back in the
 * object after the four views. Two racing, or a cube missing: as shipped.
 *
 * Measured on a desktop (Windows, Vulkan, render scale 3, SMAA; four cars
 * driving on USA's Waterfront Westbound): every frame 29.1 fps and 3,951
 * draws a frame, every 2nd 36.0 fps and 2,972, every 4th 39.4 fps and
 * 2,533. The frame is the executor's time plus the title's (a kick waits
 * for the executor, toolkit xbox_memory_layout.c), so fewer draws shorten
 * both. */
#define S4_CUBE          (S4_MGR + 0x9D0u)   /* the title's cube object */
#define S4_CUBE_TEX      0xC0u               /* the cube texture */
#define S4_CUBE_FACES    0xC8u               /* six faces */
#define S4_CUBE_OBJ      0x007BEC00u         /* views 1 to 3's cube objects, 0x100 each */
#define S4_CUBE_ON_A     0x0073A190u         /* the title's reflection gate */
#define S4_CUBE_ON_B     0x0073A194u

extern void sub_00040B90(void);

static struct { uint32_t tex, face[6]; } s_cube[SPLIT4_MAX_PLAYERS];
static int s_cube_ready;                     /* every view has its cube */
static uint32_t s_cube_frame;
static uint8_t s_cube_drawn[SPLIT4_MAX_PLAYERS];

static unsigned s4_cube_every(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("BO3_SPLIT4_CUBE");
        v = e && *e >= '1' && *e <= '8' ? *e - '0' : 4;
    }
    return (unsigned)v;
}

static void s4_cube_get(unsigned k, uint32_t obj)
{
    unsigned i;
    s_cube[k].tex = S4_M32(obj + S4_CUBE_TEX);
    for (i = 0; i < 6u; i++)
        s_cube[k].face[i] = S4_M32(obj + S4_CUBE_FACES + i * 4u);
}

static void s4_cube_set(unsigned k)
{
    unsigned i;
    S4_W32(S4_CUBE + S4_CUBE_TEX) = s_cube[k].tex;
    for (i = 0; i < 6u; i++)
        S4_W32(S4_CUBE + S4_CUBE_FACES + i * 4u) = s_cube[k].face[i];
}

/* sub_00040B90 (stdcall: the cube object): the title's cube, then three
 * more for views 1 to 3. */
void split4_cube_create(void (*gen)(void))
{
    uint32_t obj = S4_M32(g_esp + 4u);
    unsigned k, i;
    gen();
    if (obj != S4_CUBE || !split4_enabled() || s4_skip("cube"))
        return;
    s4_cube_get(0, S4_CUBE);
    s_cube_ready = s_cube[0].tex != 0;
    for (k = 1; k < SPLIT4_MAX_PLAYERS && s_cube_ready; k++) {
        uint32_t o = S4_CUBE_OBJ + (k - 1u) * 0x100u, arg = o;
        memset((uint8_t *)(uintptr_t)g_xbox_mem_offset + o, 0, 0x100u);
        s4_gcall(sub_00040B90, g_eax, g_ecx, g_edx, g_ebx, g_esi, g_edi, 1, &arg);
        s4_cube_get(k, o);
        if (!s_cube[k].tex)
            s_cube_ready = 0;
        for (i = 0; i < 6u; i++)
            if (!s_cube[k].face[i])
                s_cube_ready = 0;
    }
    fprintf(stderr, "[SPLIT4] reflection cubes per view: %s (texture %08X %08X %08X %08X)\n",
            s_cube_ready ? "made" : "not made, one cube for all views",
            s_cube[0].tex, s_cube[1].tex, s_cube[2].tex, s_cube[3].tex);
}

/* sub_0003FA20 (stdcall: the manager, a point, the view; ret 12). */
void split4_cube_draw(void (*gen)(void))
{
    uint32_t k = S4_M32(g_esp + 12u), a, b;
    unsigned n;
    if (!s_cube_ready || k >= SPLIT4_MAX_PLAYERS || !s4_four_racing()) {
        if (s_cube_ready)
            s4_cube_set(0);
        gen();
        return;
    }
    s4_cube_set(k);
    n = s4_cube_every();
    if (n <= 1u || !s_cube_drawn[k] || s_cube_frame % n == k % n) {
        s_cube_drawn[k] = 1;
        gen();
        return;
    }
    a = S4_M32(S4_CUBE_ON_A);
    b = S4_M32(S4_CUBE_ON_B);
    S4_W32(S4_CUBE_ON_A) = 0;
    S4_W32(S4_CUBE_ON_B) = 0;
    gen();
    S4_W32(S4_CUBE_ON_A) = a;
    S4_W32(S4_CUBE_ON_B) = b;
}

/* After the four views: the title's cube back in its object, the next
 * frame's turn. A race's end forgets the faces (a new race draws each
 * view's before using it). */
static void s4_cube_frame_end(void)
{
    if (!s_cube_ready)
        return;
    s4_cube_set(0);
    s_cube_frame++;
}

void split4_cube_reset(void)
{
    memset(s_cube_drawn, 0, sizeof s_cube_drawn);
    if (s_cube_ready)
        s4_cube_set(0);
}
