#include "host_stall_trace.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    HostStallTrace trace = {0};
    host_stall_trace_record(&trace, HOST_STALL_GAME, 50000000, 50000000, 0, 0, 0, 1);
    assert(trace.total == 0); /* Opt-in only. */
    host_stall_trace_start(&trace, 1000000000);
    host_stall_trace_record(&trace, HOST_STALL_GAME, 1100000000,
        HOST_STALL_TRACE_THRESHOLD_NS - 1, 27, 2, 0, 1);
    assert(trace.total == 0);
    host_stall_trace_record(&trace, HOST_STALL_AUDIO, 1150000000,
        HOST_STALL_TRACE_THRESHOLD_NS, 27, 3, 2940, 4);
    assert(trace.total == 1 && trace.records[0].stage == HOST_STALL_AUDIO);
    assert(trace.records[0].at_ns == 150000000);
    for (unsigned i = 0; i < HOST_STALL_TRACE_CAPACITY + 5; ++i) {
        host_stall_trace_record(&trace, HOST_STALL_PRESENT,
            1200000000ULL + i * 1000000ULL, 50000000, 27, i, 0, 4);
    }
    assert(trace.total == HOST_STALL_TRACE_CAPACITY + 6);
    assert(trace.records[(trace.total - 1) % HOST_STALL_TRACE_CAPACITY].entry
           == HOST_STALL_TRACE_CAPACITY + 4);
    const char *path = "host-stall-trace-test.txt";
    assert(host_stall_trace_write(&trace, path));
    FILE *file = fopen(path, "r");
    assert(file);
    char line[256];
    assert(fgets(line, sizeof(line), file));
    assert(strstr(line, "VanillaBTN"));
    assert(fgets(line, sizeof(line), file));
    assert(strstr(line, "retained=128"));
    unsigned lines = 0;
    while (fgets(line, sizeof(line), file)) ++lines;
    assert(lines == HOST_STALL_TRACE_CAPACITY);
    assert(fclose(file) == 0);
    assert(remove(path) == 0);
    puts("host_stall_trace tests passed.");
    return 0;
}
