#ifndef HMBTN_GPU_SCENE_H
#define HMBTN_GPU_SCENE_H
#include "gfx/replay_sink.h"
#include <stddef.h>
enum {
    HM_SCENE_HEADER = 16,
    HM_SCENE_STRIDE = 40,
    HM_SCENE_PRIMITIVES = 8192,
    HM_SCENE_BINS = 2048,
    HM_SCENE_BIN_WORDS = 257,
    HM_SCENE_MAX_WORDS = HM_SCENE_HEADER + 1024 * 512 / 2 + HM_SCENE_STRIDE * HM_SCENE_PRIMITIVES +
                         HM_SCENE_BINS * HM_SCENE_BIN_WORDS
};
typedef struct hm_gpu_scene hm_gpu_scene;
typedef struct {
    const u32 *words;
    size_t bytes;
    u64 revision;
    u32 width, height;
} hm_gpu_scene_data;
hm_gpu_scene *hm_gpu_scene_create(void);
void hm_gpu_scene_destroy(hm_gpu_scene *scene);
void hm_gpu_scene_texture_filter(hm_gpu_scene *scene, int linear);
void hm_gpu_scene_multisamples(hm_gpu_scene *scene, unsigned samples);
hm_replay_sink hm_gpu_scene_begin(hm_gpu_scene *scene, u32 width, u32 height);
int hm_gpu_scene_end(hm_gpu_scene *scene, hm_replay_sink previous, hm_gpu_scene_data *data);
const char *hm_gpu_scene_fallback(const hm_gpu_scene *scene);
#endif
