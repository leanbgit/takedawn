/*
 * split4.c - four-player split screen (docs/split4.md).
 */
#include "split4.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

extern ptrdiff_t g_xbox_mem_offset;
#define S4_M8(va)  (*(const uint8_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_M16(va) (*(const int16_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_M32(va) (*(const uint32_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_MF(va)  (*(const float *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))

#include "xbox_memory_layout.h"

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
    fputc('\n', s_log);
}
