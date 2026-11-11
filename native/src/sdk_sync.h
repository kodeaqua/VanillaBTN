#ifndef VANILLABTN_SDK_SYNC_H
#define VANILLABTN_SDK_SYNC_H

#include <stdbool.h>
#include <stdint.h>

/* A clock and absolute-deadline wait allow synthetic testing without sleeping.
 * Pass NULL/NULL/NULL to restore SDL's monotonic clock and wait implementation.
 * Binding starts a fresh timing epoch; repeated ResetCallback calls do not. */
bool sdk_sync_bind(uint64_t (*now)(void *), void (*wait)(uint64_t, void *), void *context);

/* Cooperative host event/presentation hook after the original SDK callback.
 * Only waiting VSync calls pump; query modes never submit another frame. */
void sdk_sync_pump(void (*pump)(void *), void *context);

/* Called while VSync is waiting. The hook may render an interpolated host
 * frame and returns its next desired wake-up time, or the VSync deadline when
 * no extra presentation is due. */
void sdk_sync_subtick_hook(uint64_t (*subtick)(uint64_t deadline, void *), void *context);

/* Turbo speed multiplier (1 = normal 60 Hz simulation, 2 = 2x, 3 = 3x, etc.) */
void sdk_sync_set_turbo(uint32_t multiplier);
uint32_t sdk_sync_get_turbo(void);

/* Cooperative cancellation allows the original loop to return on window close. */
void sdk_sync_start(void);
void sdk_sync_stop(void);
bool sdk_sync_running(void);

#endif
