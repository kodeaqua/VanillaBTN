#ifndef VANILLABTN_HOST_STALL_TRACE_H
#define VANILLABTN_HOST_STALL_TRACE_H

#include <stdbool.h>
#include <stdint.h>

/* Optional in-memory trace for one-off frame/audio stalls in Release. Keep
 * instrumentation independent of SDL, Windows headers and game data. */
#define HOST_STALL_TRACE_CAPACITY 128u
#define HOST_STALL_TRACE_THRESHOLD_NS 33333333ULL

typedef enum HostStallStage {
    HOST_STALL_GAME,
    HOST_STALL_VSYNC_WAIT,
    HOST_STALL_EVENTS,
    HOST_STALL_INPUT,
    HOST_STALL_AUDIO,
    HOST_STALL_PRESENT,
    HOST_STALL_PUMP_GAP
} HostStallStage;

typedef struct HostStallRecord {
    uint64_t at_ns, duration_ns;
    int scene, queued_bytes, scale;
    unsigned entry;
    HostStallStage stage;
} HostStallRecord;

typedef struct HostStallTrace {
    bool enabled;
    uint64_t started_ns;
    unsigned total;
    HostStallRecord records[HOST_STALL_TRACE_CAPACITY];
} HostStallTrace;

void host_stall_trace_start(HostStallTrace *trace, uint64_t now_ns);
void host_stall_trace_record(HostStallTrace *trace, HostStallStage stage,
    uint64_t now_ns, uint64_t duration_ns, int scene, unsigned entry,
    int queued_bytes, int scale);
bool host_stall_trace_write(const HostStallTrace *trace, const char *path);

#endif
