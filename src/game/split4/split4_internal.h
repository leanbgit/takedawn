/*
 * split4_internal.h - what the four-player split screen's files share
 * (docs/split4.md). Not for the rest of the game: split4.h is its face.
 *
 * Files (src/game/split4/):
 *   core.c        switches (BO3_SPLIT4, _AI, _SKIP), guest calls, start-up
 *   log.c         the state log (BO3_SPLIT4_LOG)
 *   stage.c       the four-player stage: humans, setup records, pads, keys, AI
 *   cars.c        players 3's and 4's cars in the race
 *   car_select.c  players 3's and 4's turns at the car select
 *   hud.c         players 3's and 4's HUD (a second HUD manager)
 *   camera.c      players 3's and 4's chase cameras and their tuning
 *   views.c       four views (quarters), their effects and sky
 *   streaming.c   track stream windows 3 and 4, traffic around players 3, 4
 *   sound.c       players 3's and 4's engines and listeners
 */
#ifndef BO3_SPLIT4_INTERNAL_H
#define BO3_SPLIT4_INTERNAL_H

#include "split4.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "xbox_memory_layout.h"
#include "platform/recomp_env.h"
#include "platform/recomp_patch.h"
#include "recomp_types.h"

/* ---- Guest memory and registers ---- */

extern ptrdiff_t g_xbox_mem_offset;
extern RECOMP_TLS uint32_t g_eax, g_ebx, g_ecx, g_edx, g_esi, g_edi, g_esp;

#define S4_M8(va)  (*(const uint8_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_M16(va) (*(const int16_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_M32(va) (*(const uint32_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_MF(va)  (*(const float *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))

#define S4_W8(va)  (*(uint8_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_W16(va) (*(uint16_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_W32(va) (*(uint32_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define S4_WF(va)  (*(float *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))

/* ---- The title's objects more than one file uses ---- */

#define S4_MODULE_CUR    0x004D5370u   /* the current module */
#define S4_VT_STAGE_2P   0x003A939Cu   /* the two-player stage module */
#define S4_STAGE_HUMANS  0xA8u         /* the module's racer counts (sub_00017450 */
#define S4_STAGE_AIS     0xACu         /*  passes them to sub_001AACB0) */
#define S4_HUMAN_VT      0x003B11A0u   /* CB3PlayerCar */
#define S4_SETUP         0x0078A000u   /* relocated setup records, 0x24 each */
#define S4_SETUP_SIZE    0x24u
#define S4_CAM           0x0078A100u   /* relocated player cameras, 0xC50 each */
#define S4_CAM_SIZE      0x0C50u
#define S4_HUD           0x0054F900u   /* the HUD manager, its player records first */
#define S4_HUD_REC       0x70u
#define S4_PHYS          0x007A0000u   /* relocated human physics objects */
#define S4_PHYS_SIZE     0x4AD0u
#define S4_WINDOW        0x00793000u   /* relocated stream windows (viewdata) */
#define S4_WINDOW_SIZE   0x1CCu
#define S4_MGR           0x004D6170u   /* the render manager */

/* a human racer's fields */
#define S4_CAR_AI        0x27D8u       /* byte: the AI drives this car */
#define S4_CAR_PHASE     0x134Cu       /* 2: in play */
#define S4_CAR_AI_RESET  0x19D0u
#define S4_CAR_DRIVER    0x1A00u

/* ---- core.c ---- */

/* BO3_SPLIT4_AI: bit 0 the AI drives player 3, bit 1 player 4 */
int s4_ai34(void);
/* the current module is the two-player stage */
int s4_stage_is_2p(void);
/* four humans race on the two-player stage */
int s4_four_racing(void);
/* BO3_SPLIT4_SKIP=<names>: development switch, the named part stays off */
int s4_skip(const char *part);

/* A guest call: these registers, these pushes (in push order), a return
 * address of 0; every register and esp as they were after. */
void s4_gcall(void (*fn)(void), uint32_t eax, uint32_t ecx, uint32_t edx,
              uint32_t ebx, uint32_t esi, uint32_t edi,
              unsigned n, const uint32_t *push);
/* A guest call with eax set and the other registers as they are; its eax
 * back in *r (when r). */
void s4_gcall_r(void (*fn)(void), uint32_t eax, unsigned n, const uint32_t *push,
                uint32_t *r);
/* A guest virtual call without stack arguments: ecx the object, the
 * function at its vtable + off; registers and esp as they were (_r: its
 * eax back). */
void s4_vcall(uint32_t obj, uint32_t off);
uint32_t s4_vcall_r(uint32_t obj, uint32_t off);

/* ---- views.c ---- */

uint32_t s4_view(unsigned k);  /* player k's view */
uint32_t s4_fx(unsigned k);    /* player k's effects record */

/* ---- cars.c ---- */

void s4_car_name(uint32_t lo, uint32_t hi, char *out);  /* a car id's name, 9 bytes */

/* ---- streaming.c ---- */

void s4_streams_reset(void);   /* windows 2 and 3 stopped, their pools free */

/* ---- each file's marks (config/patches.json), registered by split4_start ---- */

void s4_stage_start_marks(void);
void s4_camera_start_marks(void);
void s4_views_start_marks(void);
void s4_streaming_start_marks(void);
void s4_sound_start_marks(void);

#endif
