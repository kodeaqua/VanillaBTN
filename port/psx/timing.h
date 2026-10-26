#ifndef HMBTN_PSX_TIMING_H
#define HMBTN_PSX_TIMING_H

#include "compat/types.h"

/* Host hooks run only on the game thread. wait_tick waits/pumps events for the
 * next 60 Hz tick; scanlines measures elapsed host time for profiling queries. */
typedef struct {
    void *context;
    void (*wait_tick)(void *context, u64 tick);
    s32 (*scanlines)(void *context);
} hm_timing_host;
void hm_timing_reset(hm_timing_host host);
u64 hm_timing_ticks(void);

#endif
