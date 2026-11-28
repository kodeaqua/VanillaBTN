#ifndef FRAME_BUFFERS_H
#define FRAME_BUFFERS_H

/* The frame records: linking primitives into the OT, the debug hook, and laying out
 * both frames' OTs and primitive buffers. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

#define FRAME_WORK_BYTES 0x2EE0
#define FRAME_WIDE_WORK_BYTES 0x5EDC

#ifdef __psyz
void frame_buffers_set_work(s32 index, u32 bytes);
#endif
void *ot_insert_prim(s32 index, s32 advance);
void debug_hook_run(s32 unused0);
void frame_buffers_layout(void);

#endif /* FRAME_BUFFERS_H */
