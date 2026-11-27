#ifndef OVERLAY_REGIONS_H
#define OVERLAY_REGIONS_H


/* The overlays' two load regions. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

extern u8 *const overlay_region_table[2];

/* Bytes from the frame work region to the end of its scratch area. */
#define FRAME_REGION_BYTES 0x3EE50
#ifdef __psyz
extern u8 *const native_work_region;
#endif

/* The overlays' two load regions: low for FARM and the minigames, high for saveload and swind. */
enum { OVERLAY_LOW, OVERLAY_HIGH };

#endif /* OVERLAY_REGIONS_H */
