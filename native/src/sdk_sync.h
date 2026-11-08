#ifndef VANILLABTN_SDK_SYNC_H
#define VANILLABTN_SDK_SYNC_H

#include <stdbool.h>
#include <stdint.h>

/* A clock and absolute-deadline wait allow synthetic testing without sleeping.
 * Pass NULL/NULL/NULL to restore SDL's monotonic clock and wait implementation.
 * Binding starts a fresh timing epoch; repeated ResetCallback calls do not. */
bool sdk_sync_bind(uint64_t (*now)(void *), void (*wait)(uint64_t, void *), void *context);

#endif
