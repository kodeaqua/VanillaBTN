#include "platform/input_replay.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
int hm_input_replay_line(hm_input_replay *replay, const char *line) {
    while (isspace((unsigned char)*line))
        ++line;
    if (!*line || *line == '#')
        return 1;
    if (!isdigit((unsigned char)*line) || replay->count == 1024)
        return 0;
    unsigned long long tick;
    unsigned int mask;
    char trailing;
    if (sscanf(line, "%llu %x %c", &tick, &mask, &trailing) != 2 || mask > 65535 || !tick)
        return 0;
    if (replay->count && tick <= replay->changes[replay->count - 1].tick)
        return 0;
    replay->changes[replay->count].tick = (u64)tick;
    replay->changes[replay->count++].held = (u16)mask;
    return 1;
}
u16 hm_input_replay_sample(hm_input_replay *replay, u64 tick) {
    while (replay->cursor < replay->count && replay->changes[replay->cursor].tick <= tick)
        replay->held = replay->changes[replay->cursor++].held;
    return replay->held;
}
int hm_input_replay_load(hm_input_replay *replay, const char *path) {
    memset(replay, 0, sizeof(*replay));
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;
    char line[128];
    int valid = 1;
    while (fgets(line, sizeof(line), file)) {
        if ((!strchr(line, '\n') && !feof(file)) || !hm_input_replay_line(replay, line)) {
            valid = 0;
            break;
        }
    }
    if (ferror(file))
        valid = 0;
    fclose(file);
    return valid;
}
