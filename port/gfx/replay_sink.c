#include "gfx/replay_sink.h"
static hm_replay_sink current;
hm_replay_sink hm_replay_bind(hm_replay_sink sink) {
    hm_replay_sink previous = current;
    current = sink;
    return previous;
}
void hm_replay_begin(const RECT *area) {
    if (current.begin)
        current.begin(current.context, area);
}
void hm_replay_command(const u32 *words, u32 available, int after) {
    if (current.command)
        current.command(current.context, words, available, after);
}
u32 hm_replay_consume(const u32 *words, u32 available) {
    return current.consume ? current.consume(current.context, words, available) : 0;
}
void hm_replay_motion(const u32 *previous, const u32 *next, u32 count, u32 alpha) {
    if (current.motion)
        current.motion(current.context, previous, next, count, alpha);
}
