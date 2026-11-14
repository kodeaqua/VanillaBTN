#ifndef VANILLABTN_HOST_PACING_H
#define VANILLABTN_HOST_PACING_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

typedef struct HostPacing {
    int target_fps;             /* Target FPS: 30, 60, monitor Hz, or 0 if unlocked */
    uint64_t frame_interval_ns; /* Target interval in ns (0 if unlocked) */
    uint64_t last_present_time_ns;
    uint64_t frames_presented;
    uint64_t simulation_ticks;
} HostPacing;

/* Initialize pacing state with display config and monitor refresh rate */
void host_pacing_init(HostPacing *pacing, const DisplayConfig *config, float monitor_hz);

/* Test if frame presentation is due at timestamp now_ns */
bool host_pacing_should_present(HostPacing *pacing, uint64_t now_ns);

/* Record frame presentation at timestamp now_ns */
void host_pacing_record_presentation(HostPacing *pacing, uint64_t now_ns);

/* Record authoritative simulation tick (60 virtual vblanks/sec) */
void host_pacing_record_tick(HostPacing *pacing);

/* Determine next wait deadline in nanoseconds between now_ns and simulation_deadline */
uint64_t host_pacing_next_wait_deadline(const HostPacing *pacing, uint64_t now_ns, uint64_t simulation_deadline);

#endif
