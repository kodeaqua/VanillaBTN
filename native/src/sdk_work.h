#ifndef VANILLABTN_SDK_WORK_H
#define VANILLABTN_SDK_WORK_H

#include <stdbool.h>
#include <stddef.h>

/* Host storage for the original primitive builders' GS work allocation calls.
 * The arena must outlive all queued packets. Bind NULL/0 on shutdown. */
bool sdk_work_bind(void *memory, size_t size);

#endif
