/*
 * core.c - the four-player split screen's switches (BO3_SPLIT4,
 * BO3_SPLIT4_AI, BO3_SPLIT4_SKIP), the guest call helpers the other files
 * use, and start-up (docs/split4.md).
 */
#include "split4_internal.h"

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
int s4_ai34(void)
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

int s4_stage_is_2p(void)
{
    uint32_t mod = S4_M32(S4_MODULE_CUR);
    return mod >= 0x10000u && mod < 0x04000000u && S4_M32(mod) == S4_VT_STAGE_2P;
}

/* BO3_SPLIT4_SKIP=<names>: development switch, the listed parts stay off:
 * views, select, quad, render, full (views.c), bind (camera.c), hud2,
 * layout, clip (hud.c), cars (cars.c), carsel (car_select.c), engine34,
 * listen34 (sound.c), traffic34 (streaming.c). */
int s4_skip(const char *part)
{
    const char *v = getenv("BO3_SPLIT4_SKIP");
    return v && strstr(v, part) != NULL;
}

int s4_four_racing(void)
{
    return split4_enabled() && s4_stage_is_2p() &&
           S4_M32(SPLIT4_WORLD + 0x12B7C0u) == SPLIT4_MAX_PLAYERS;
}

/* A guest call: these registers, these pushes (in push order), a return
 * address of 0; every register and esp as they were after. */
void s4_gcall(void (*fn)(void), uint32_t eax, uint32_t ecx, uint32_t edx,
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
void s4_gcall_r(void (*fn)(void), uint32_t eax, unsigned n, const uint32_t *push,
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

uint32_t s4_vcall_r(uint32_t obj, uint32_t off)
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

/* A guest virtual call without stack arguments: ecx the object, the
 * function at its vtable + off. Registers and esp as they were. */
void s4_vcall(uint32_t obj, uint32_t off)
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

/* At start-up, before the guest runs (main.c): each file's marks. */
void split4_start(void)
{
    if (!split4_enabled())
        return;
    s4_streaming_start_marks();
    s4_camera_start_marks();
    s4_views_start_marks();
    s4_sound_start_marks();
    s4_stage_start_marks();
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
