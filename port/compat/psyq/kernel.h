#ifndef HMBTN_COMPAT_KERNEL_H
#define HMBTN_COMPAT_KERNEL_H

#include "compat/types.h"

/* Source-facing directory metadata, never read as an SDK or memory-card struct.
 * Twenty card filename bytes plus a terminator are owned by the native layer. */
struct DIRENTRY {
    char name[21];
    s32 size;
};

#endif
