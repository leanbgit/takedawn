/*
 * d3dswm.c - what Burnout 3's D3D does for the GPU's software methods, done
 * on the executor's thread (the toolkit roadmap's step 4).
 *
 * XDK D3D writes NO_OPERATION with a non-zero parameter into its push buffer
 * as a software method: the GPU stops there and raises an interrupt, and
 * D3D's handler (sub_00357000 -> sub_00357250, a 14-case table at
 * 0x00357464 picked by the parameter's low five bits) does the work. The
 * toolkit runs the swap (case 1) itself and hands the rest to this
 * (nv2a_pb_set_swmethod_handler; nv2a_pb_exec.c, software_method).
 *
 * Case 5 (0x003573BE): KeSetEvent(device + 0x1DCC). BlockOnTime
 * (sub_00351770), waiting on a fence the GPU is still 9 KB or more away
 * from, writes WAIT_FOR_IDLE and NOP 5 over the fence's padding, kicks, and
 * sleeps on that event (0x003518B4) until the GPU gets there. The interrupt
 * handler's context is device + 0x1C28 (its first word is the register base,
 * as [device + 0x1C28] is in KickOff, sub_00351BD0), and it sets the event
 * at context + 0x1A4.
 *
 * It never happens while the GPU finishes each segment before the title may
 * kick the next (today's handshake): after BlockOnTime's kick the GPU is
 * past every fence. It is what a GPU running behind the title (the
 * roadmap's step 5) needs, or the title sleeps for ever.
 *
 * The other cases (callbacks, PGRAPH writes, the swap's counters) are not
 * done here: the toolkit counts and names any that arrive.
 */
#include <stdint.h>
#include <stdio.h>

#include "recomp_types.h"
#include "region.h"
#include "kernel/kernel.h"            /* xbox_KeSetEventHost */
#include "nv2a_pb_state.h"            /* nv2a_pb_set_swmethod_handler */

#define D3D_DEVICE      BO3_VA(0x35FB48u, 0x35FE48u, 0x35FEE8u)  /* D3D__pDevice */
#define DEV_GPU_EVENT   0x1DCCu       /* BlockOnTime's KEVENT */

static int bo3_swmethod(uint32_t subch, uint32_t param)
{
    (void)subch;
    switch (param & 0x1Fu) {
    case 5: {
        uint32_t dev = MEM32(D3D_DEVICE);
        if (dev && xbox_KeSetEventHost(dev + DEV_GPU_EVENT))
            return 1;
        {
            static int said;
            if (!said++)
                fprintf(stderr, "[D3DSWM] software method 5: no KEVENT at device"
                        " 0x%08X + 0x%X\n", dev, DEV_GPU_EVENT);
        }
        return 0;
    }
    default:
        return 0;
    }
}

void bo3_d3dswm_init(void)
{
    nv2a_pb_set_swmethod_handler(bo3_swmethod);
}
