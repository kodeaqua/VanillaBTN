#ifndef HMBTN_SOFTWARE_OUTPUT_H
#define HMBTN_SOFTWARE_OUTPUT_H
#include "gfx/frame_events.h"
#include "gfx/replay_sink.h"
typedef struct hm_software_output hm_software_output;
typedef struct {
    const u16 *pixels;
    u32 width, height;
} hm_software_view;
hm_software_output *hm_software_output_create(void);
void hm_software_output_destroy(hm_software_output *output);
void hm_software_output_filter(hm_software_output *output, int linear);
int hm_software_output_prepare(hm_software_output *output, u32 width, u32 height);
hm_replay_sink hm_software_output_begin(hm_software_output *output);
int hm_software_output_replay(hm_software_output *output, const hm_render_frame *live);
int hm_software_output_end(hm_software_output *output, hm_replay_sink previous,
                           hm_software_view *view);
#endif
