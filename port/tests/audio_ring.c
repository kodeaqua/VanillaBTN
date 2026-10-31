#include "platform/audio_ring.h"
#include <stdio.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static hm_audio_ring ring;
static s16 full[HM_AUDIO_RING_FRAMES * 2];
int main(void) {
    hm_audio_ring_init(&ring, 4);
    const s16 input[] = {0, 0, 1000, -1000, 2000, -2000, 3000, -3000, 4000, -4000};
    float output[12];
    hm_audio_ring_read(&ring, output, 4, 2, 44100);
    CHECK(!output[0] && !ring.primed);
    CHECK(hm_audio_ring_push(&ring, input, 5) == 5);
    hm_audio_ring_read(&ring, output, 3, 2, 44100);
    CHECK(output[0] == 0 && output[2] == 1000.0f / 32768 && output[5] == -2000.0f / 32768);
    hm_audio_ring_init(&ring, 2);
    hm_audio_ring_push(&ring, input, 5);
    hm_audio_ring_read(&ring, output, 6, 2, 88200);
    CHECK(output[2] == 500.0f / 32768 && output[4] == 1000.0f / 32768 &&
          output[10] == 2500.0f / 32768);
    hm_audio_ring_init(&ring, 2);
    CHECK(hm_audio_ring_push(&ring, full, HM_AUDIO_RING_FRAMES) == HM_AUDIO_RING_FRAMES);
    CHECK(hm_audio_ring_push(&ring, input, 1) == 0 && atomic_load(&ring.dropped) == 1);
    for (u32 i = 0; i < 10000; ++i) {
        hm_audio_ring_read(&ring, output, 1, 2, 44100);
        hm_audio_ring_push(&ring, input, 1);
    }
    CHECK(atomic_load(&ring.write) > HM_AUDIO_RING_FRAMES);
    CHECK(!atomic_load(&ring.underflows));

    hm_audio_ring_init(&ring, 2);
    hm_audio_ring_push(&ring, input, 5);
    hm_audio_ring_read(&ring, output, 6, 2, 44100);
    CHECK(atomic_load(&ring.underflows) > 0);
    return 0;
}
