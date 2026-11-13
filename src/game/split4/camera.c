/*
 * camera.c - players 3's and 4's chase cameras: their mode switches,
 * frame and reset, their tuning per car, their bind to their views
 * (docs/split4.md, cameras).
 */
#include "split4_internal.h"

extern void sub_00167940(void);        /* a player's cameras' frame (esi the camera; time, ret 4) */

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

/* ---- The chase camera's tuning, per car ----
 *
 * A camera's bind (sub_001674B0) takes its car's camera tuning from
 * sub_00160690 (cdecl: the car's id, low and high word; the entry back):
 * two entries (0x1B0 each, at [0x47A134] + 0x28D0 and + 0x2A80, the car's
 * id at + 0xA0, + 0xA4) for the two players, one found by the id or a free
 * one filled (sub_00160840, eax the entry: the car's tunables bound by
 * name), else none (0). With four, a car that is neither player 1's nor
 * player 2's (nor in a free entry) gets none, and its camera sat in the car
 * (camera + 0x4CC 0: the eye at the car). Players 3 and 4 get two more
 * entries here; sub_00160710 (the same arguments: the entry let go,
 * sub_00160AD0 with eax the entry) lets ours go too. */
#define S4_CAMDB         0x007BE500u   /* two more entries */
#define S4_CAMDB_SIZE    0x000001B0u
#define S4_CAMDB_BASE    0x0047A134u

extern void sub_00160840(void), sub_00160AD0(void);

static int s4_camdb_has(uint32_t e, uint32_t lo, uint32_t hi)
{
    return S4_M32(e + 0xA0u) == lo && S4_M32(e + 0xA4u) == hi;
}

void split4_camdb_get(void (*gen)(void))
{
    uint32_t lo = S4_M32(g_esp + 4u), hi = S4_M32(g_esp + 8u), k, e;

    gen();
    if (g_eax || !split4_enabled() || !(lo | hi))
        return;
    for (k = 0; k < 2u; k++) {
        e = S4_CAMDB + k * S4_CAMDB_SIZE;
        if (s4_camdb_has(e, lo, hi)) {
            g_eax = e;
            return;
        }
    }
    for (k = 0; k < 2u; k++) {
        e = S4_CAMDB + k * S4_CAMDB_SIZE;
        if (!(S4_M32(e + 0xA0u) | S4_M32(e + 0xA4u))) {
            S4_W32(e + 0xA0u) = lo;
            S4_W32(e + 0xA4u) = hi;
            char name[16];
            s4_gcall_r(sub_00160840, e, 0, NULL, NULL);
            s4_car_name(lo, hi, name);
            fprintf(stderr, "[SPLIT4] camera tuning for %s in entry %u (0x%08X)\n",
                    name, k + 2u, e);
            g_eax = e;
            return;
        }
    }
}

void split4_camdb_put(void (*gen)(void))
{
    uint32_t lo = S4_M32(g_esp + 4u), hi = S4_M32(g_esp + 8u), base = S4_M32(S4_CAMDB_BASE), k, e;

    if (split4_enabled() && (lo | hi) && base
        && !s4_camdb_has(base + 0x28D0u, lo, hi) && !s4_camdb_has(base + 0x2A80u, lo, hi))
        for (k = 0; k < 2u; k++) {
            e = S4_CAMDB + k * S4_CAMDB_SIZE;
            if (s4_camdb_has(e, lo, hi)) {
                S4_W32(e + 0xA0u) = 0;
                S4_W32(e + 0xA4u) = 0;
                s4_gcall_r(sub_00160AD0, e, 0, NULL, NULL);
                g_esp += 4u;                    /* ret */
                return;
            }
        }
    gen();
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

void s4_camera_start_marks(void)
{
    recomp_patch_on_mark("s4_cam_frame", split4_cam_frame);
    recomp_patch_on_mark("s4_cam_reset", split4_cam_reset);
}
