#include "gfx/software_output.h"
#include "gfx/frame_replay.h"
#include "gfx/software_raster.h"
#include <stdlib.h>
struct hm_software_output {
    hm_frame_replay *replay;
    hm_software_surface surface;
    u64 cached_serial, cached_revision;
};
hm_software_output *hm_software_output_create(void) {
    hm_software_output *s = calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->replay = hm_frame_replay_acquire();
    if (!s->replay) {
        free(s);
        return NULL;
    }
    return s;
}
void hm_software_output_destroy(hm_software_output *s) {
    if (s) {
        hm_frame_replay_release(s->replay);
        free(s->surface.pixels);
        free(s);
    }
}
void hm_software_output_filter(hm_software_output *s, int filter_mode) {
    if (filter_mode < 0 || filter_mode > 2)
        filter_mode = 0;
    if (s && s->surface.texture_filter != filter_mode) {
        s->surface.texture_filter = filter_mode;
        s->cached_serial = 0;
    }
}
int hm_software_output_prepare(hm_software_output *s, u32 width, u32 height) {
    if (!s || !width || !height || width > 4096 || height > 4096)
        return 0;
    if (s->surface.width == width && s->surface.height == height)
        return 1;
    u16 *pixels = malloc((size_t)width * height * sizeof(*pixels));
    if (!pixels)
        return 0;
    int filter_mode = s->surface.texture_filter;
    free(s->surface.pixels);
    s->surface = (hm_software_surface){
        .pixels = pixels, .width = width, .height = height, .texture_filter = filter_mode};
    s->cached_serial = 0;
    return 1;
}
hm_replay_sink hm_software_output_begin(hm_software_output *s) {
    if (!s)
        return hm_replay_bind((hm_replay_sink){0});
    s->surface.begun = 0;
    return hm_replay_bind(hm_software_sink(&s->surface));
}
int hm_software_output_replay(hm_software_output *s, const hm_render_frame *live) {
    if (!s || live->bypass)
        return 0;
    u64 serial = hm_frame_replay_serial(s->replay, live);
    if (!serial)
        return 0;
    if (serial == s->cached_serial && s->surface.revision == s->cached_revision) {
        s->surface.begun = 1;
        return 1;
    }
    if (!hm_frame_replay_draw(s->replay, live))
        return 0;
    if (s->surface.begun) {
        s->cached_serial = serial;
        s->cached_revision = s->surface.revision;
    }
    return s->surface.begun;
}
int hm_software_output_end(hm_software_output *s, hm_replay_sink previous, hm_software_view *view) {
    hm_replay_bind(previous);
    if (!s || !s->surface.begun)
        return 0;
    *view = (hm_software_view){s->surface.pixels, s->surface.width, s->surface.height};
    return 1;
}
