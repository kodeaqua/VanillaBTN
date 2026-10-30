#include "enhance/scaling/scaling.h"
static hm_software_output *hm_scaling_output;
int hm_scaling_enable(int enabled) {
    if (!enabled) {
        hm_software_output_destroy(hm_scaling_output);
        hm_scaling_output = NULL;
        return 1;
    }
    if (!hm_scaling_output)
        hm_scaling_output = hm_software_output_create();
    return hm_scaling_output != NULL;
}
void hm_scaling_filter(int filter_mode) { hm_software_output_filter(hm_scaling_output, filter_mode); }
int hm_scaling_prepare(u32 width, u32 height) {
    return hm_software_output_prepare(hm_scaling_output, width, height);
}
hm_replay_sink hm_scaling_begin(void) { return hm_software_output_begin(hm_scaling_output); }
int hm_scaling_replay(const hm_render_frame *live) {
    return hm_software_output_replay(hm_scaling_output, live);
}
int hm_scaling_end(hm_replay_sink previous, hm_scaled_view *view) {
    return hm_software_output_end(hm_scaling_output, previous, view);
}
