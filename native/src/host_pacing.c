#include "host_pacing.h"
#include <string.h>

void host_pacing_init(HostPacing *pacing, const DisplayConfig *config, float monitor_hz) {
    if (!pacing) return;
    memset(pacing, 0, sizeof(*pacing));

    int limit = config ? config->fps_limit : 0;
    if (limit == 0) {
        /* Follow monitor refresh rate */
        if (monitor_hz >= 20.0f && monitor_hz <= 1000.0f) {
            pacing->target_fps = (int)(monitor_hz + 0.5f);
        } else {
            pacing->target_fps = 60;
        }
    } else if (limit == -1) {
        /* Unlocked */
        pacing->target_fps = 0;
    } else if (limit > 0) {
        pacing->target_fps = limit;
    } else {
        pacing->target_fps = 60;
    }

    if (pacing->target_fps > 0) {
        pacing->frame_interval_ns = 1000000000ULL / (uint64_t)pacing->target_fps;
    } else {
        pacing->frame_interval_ns = 0; /* Unlocked */
    }
}

bool host_pacing_should_present(HostPacing *pacing, uint64_t now_ns) {
    if (!pacing) return true;
    if (pacing->frame_interval_ns == 0) return true; /* Unlocked */
    if (pacing->frames_presented == 0) return true; /* First frame */

    /* Allow a small margin (0.5 ms) to prevent jitter from pushing a frame to next cycle */
    const uint64_t margin = 500000ULL; /* 0.5 ms */
    /* Compare elapsed time instead of adding absolute timestamps. A huge
     * uptime or mock timestamp near UINT64_MAX must not wrap the deadline. */
    if (now_ns < pacing->last_present_time_ns) return true;
    uint64_t elapsed = now_ns - pacing->last_present_time_ns;
    uint64_t threshold = pacing->frame_interval_ns > margin
        ? pacing->frame_interval_ns - margin : pacing->frame_interval_ns;
    return elapsed >= threshold;
}

void host_pacing_record_presentation(HostPacing *pacing, uint64_t now_ns) {
    if (!pacing) return;
    pacing->last_present_time_ns = now_ns;
    pacing->frames_presented++;
}

void host_pacing_record_tick(HostPacing *pacing) {
    if (!pacing) return;
    pacing->simulation_ticks++;
}

uint64_t host_pacing_next_wait_deadline(const HostPacing *pacing, uint64_t now_ns, uint64_t simulation_deadline) {
    if (!pacing || pacing->frame_interval_ns == 0) {
        return simulation_deadline;
    }
    if (pacing->frames_presented == 0) {
        return simulation_deadline;
    }
    /* Saturate the absolute deadline rather than wrapping to an early time. */
    uint64_t next_present = UINT64_MAX - pacing->last_present_time_ns
        < pacing->frame_interval_ns ? UINT64_MAX
        : pacing->last_present_time_ns + pacing->frame_interval_ns;
    if (next_present > now_ns && next_present < simulation_deadline) {
        return next_present;
    }
    return simulation_deadline;
}
