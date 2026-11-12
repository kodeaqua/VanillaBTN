#include "host_audio_policy.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "host audio policy assertion failed at line %d: %s\n", \
            __LINE__, #condition); exit(1); \
} } while (0)

static void test_default_is_bit_exact(void) {
    int16_t samples[] = {INT16_MIN, -12345, -1, 0, 1, 12345, INT16_MAX};
    int16_t original[sizeof(samples) / sizeof(samples[0])];
    memcpy(original, samples, sizeof(samples));
    host_audio_apply_volume(samples, 7, 100, false);
    CHECK(memcmp(samples, original, sizeof(samples)) == 0);
    host_audio_apply_volume(NULL, 7, 50, false);
    host_audio_apply_volume(samples, 0, 0, true);
    CHECK(memcmp(samples, original, sizeof(samples)) == 0);
}

static void test_master_volume_and_focus_mute(void) {
    int16_t samples[] = {INT16_MIN, -10000, -1, 0, 1, 10000, INT16_MAX};
    host_audio_apply_volume(samples, 7, 50, false);
    CHECK(samples[0] == -16384 && samples[1] == -5000);
    CHECK(samples[2] == 0 && samples[3] == 0 && samples[4] == 0);
    CHECK(samples[5] == 5000 && samples[6] == 16383);
    host_audio_apply_volume(samples, 7, 100, true);
    for (int i = 0; i < 7; ++i) CHECK(samples[i] == 0);
    samples[0] = INT16_MAX;
    host_audio_apply_volume(samples, 7, 0, false);
    CHECK(samples[0] == 0);
    samples[0] = INT16_MIN;
    host_audio_apply_volume(samples, 7, 300, false);
    CHECK(samples[0] == INT16_MIN); /* Defensive range handling */
}

int main(void) {
    test_default_is_bit_exact();
    test_master_volume_and_focus_mute();
    puts("host_audio_policy_test: passed");
    return 0;
}
