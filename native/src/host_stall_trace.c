#include "host_stall_trace.h"
#include <stdio.h>
#include <string.h>

void host_stall_trace_start(HostStallTrace *trace, uint64_t now_ns) {
    if (!trace) return;
    memset(trace, 0, sizeof(*trace));
    trace->started_ns = now_ns;
    trace->enabled = true;
}

void host_stall_trace_record(HostStallTrace *trace, HostStallStage stage,
    uint64_t now_ns, uint64_t duration_ns, int scene, unsigned entry,
    int queued_bytes, int scale) {
    if (!trace || !trace->enabled || duration_ns < HOST_STALL_TRACE_THRESHOLD_NS) return;
    unsigned slot = trace->total % HOST_STALL_TRACE_CAPACITY;
    trace->records[slot] = (HostStallRecord){
        now_ns >= trace->started_ns ? now_ns - trace->started_ns : 0,
        duration_ns, scene, queued_bytes, scale, entry, stage
    };
    ++trace->total;
}

static const char *stage_name(HostStallStage stage) {
    switch (stage) {
    case HOST_STALL_GAME: return "game";
    case HOST_STALL_VSYNC_WAIT: return "vsync_wait";
    case HOST_STALL_EVENTS: return "events";
    case HOST_STALL_INPUT: return "input";
    case HOST_STALL_AUDIO: return "audio";
    case HOST_STALL_PRESENT: return "present";
    case HOST_STALL_PUMP_GAP: return "pump_gap";
    default: return "unknown";
    }
}

bool host_stall_trace_write(const HostStallTrace *trace, const char *path) {
    if (!trace || !trace->enabled || !path) return false;
    FILE *file = fopen(path, "w");
    if (!file) return false;
    fprintf(file, "VanillaBTN optimized Debug stall trace\n");
    fprintf(file, "threshold_ms=33.333 recorded=%u retained=%u\n",
        trace->total, trace->total < HOST_STALL_TRACE_CAPACITY
            ? trace->total : HOST_STALL_TRACE_CAPACITY);
    unsigned first = trace->total > HOST_STALL_TRACE_CAPACITY
        ? trace->total - HOST_STALL_TRACE_CAPACITY : 0;
    for (unsigned i = first; i < trace->total; ++i) {
        const HostStallRecord *r = &trace->records[i % HOST_STALL_TRACE_CAPACITY];
        fprintf(file, "at_ms=%.3f stage=%s duration_ms=%.3f scene=%d entry=%u queue_bytes=%d scale=%d\n",
            (double)r->at_ns / 1000000.0, stage_name(r->stage),
            (double)r->duration_ns / 1000000.0, r->scene,
            r->entry, r->queued_bytes, r->scale);
    }
    return fclose(file) == 0;
}
