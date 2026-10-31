#ifndef HMBTN_FRAME_REPLAY_H
#define HMBTN_FRAME_REPLAY_H
#include "gfx/frame_events.h"
typedef struct hm_frame_replay hm_frame_replay;
/* Shared read-only capture for output backends. Reference-counted on the game
 * thread so native GPU rendering does not depend on any enhancement setting. */
hm_frame_replay *hm_frame_replay_acquire(void);
void hm_frame_replay_release(hm_frame_replay *replay);
u64 hm_frame_replay_serial(hm_frame_replay *replay, const hm_render_frame *live);
int hm_frame_replay_draw(hm_frame_replay *replay, const hm_render_frame *live);
const char *hm_frame_replay_failure(const hm_frame_replay *replay);
#endif
