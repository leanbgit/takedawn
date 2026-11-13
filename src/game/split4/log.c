/*
 * log.c - the four-player state log, BO3_SPLIT4_LOG (docs/split4.md,
 * "Checks").
 */
#include "split4_internal.h"

#include <math.h>

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
        fprintf(s_log, " %d/%d/%u", (int32_t)S4_M32(0x00793000u + k * 0x1CCu),
                (int32_t)S4_M32(0x00793000u + k * 0x1CCu + 4u), S4_M32(0x00793000u + k * 0x1CCu + 0x10u));
    /* Each player's controller record (setup + 0: 0x4AE724 + 0x188 n, the
     * pad of the title's port n). */
    /* Each player camera's view (+ 0xC3C: 2 and 5 the chase views), state
     * (+ 0xC20) and its eye (+ 0x280) from the car: height, distance. */
    fprintf(s_log, " | cam");
    for (k = 0; k < SPLIT4_MAX_PLAYERS; k++) {
        uint32_t c = 0x0078A100u + k * 0x0C50u, h = SPLIT4_HUMAN + k * SPLIT4_HUMAN_SIZE;
        float dx = S4_MF(c + 0x280u) - S4_MF(h + 0x40u), dy = S4_MF(c + 0x284u) - S4_MF(h + 0x44u),
              dz = S4_MF(c + 0x288u) - S4_MF(h + 0x48u);
        fprintf(s_log, " %d/%d/%.1f/%.1f", (int32_t)S4_M32(c + 0xC3Cu), (int32_t)S4_M32(c + 0xC20u),
                dy, sqrtf(dx * dx + dz * dz));
    }
    fprintf(s_log, " | pad");
    for (k = 0; k < SPLIT4_MAX_PLAYERS; k++)
        fprintf(s_log, " %d", (int)(S4_M32(0x0078A000u + k * 0x24u) - 0x004AE724u) / 0x188);
    fputc('\n', s_log);
}
