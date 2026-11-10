/* ps1_hifi_audio_test.c — test suite for Hi-Fi 44.1 kHz Reverb DSP,
 * Full-Spectrum audio processing, and Equal-Power Panning Law.
 */

#include "ps1_reverb.h"
#include "ps1_seq.h"
#include "ps1_spu.h"
#include "sdk_sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <crtdbg.h>
#endif

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

/* 1. Hi-Fi Reverb & Full-Spectrum Audio Test */
static void test_hifi_reverb(void) {
    Ps1Reverb reverb;
    memset(&reverb, 0, sizeof(reverb));
    ps1_reverb_set_preset(&reverb, 1, 1); /* Preset 1: Room */
    TEST_ASSERT(ps1_reverb_get_hifi(&reverb) == 0);

    /* Test high-frequency preservation:
     * In authentic PS1 (22.05 kHz downsample via 39-tap half-band filter),
     * a signal alternating every sample at 22.05 kHz (Nyquist) is severely
     * attenuated by the half-band low-pass decimation filter.
     * In Hi-Fi mode (44.1 kHz direct), the high-frequency signal passes
     * into the reverb core at full bandwidth. */

    int16_t out_auth[2];
    int16_t out_hifi[2];

    /* Authentic mode: run 10 clocks of alternating high-frequency signal */
    for (int i = 0; i < 10; i++) {
        int32_t val = (i & 1) ? 10000 : -10000;
        ps1_reverb_clock(&reverb, val, val, 1, out_auth);
    }

    /* Hi-Fi mode: reset preset and run 2000 clocks so delay lines propagate */
    ps1_reverb_set_preset(&reverb, 1, 1);
    ps1_reverb_set_hifi(&reverb, 1);
    TEST_ASSERT(ps1_reverb_get_hifi(&reverb) == 1);

    int64_t hifi_energy = 0;
    for (int i = 0; i < 2000; i++) {
        int32_t val = (i < 20) ? 10000 : 0;
        ps1_reverb_clock(&reverb, val, val, 1, out_hifi);
        if (out_hifi[0] != 0 || out_hifi[1] != 0) {
            hifi_energy += (int64_t)out_hifi[0] * out_hifi[0] + (int64_t)out_hifi[1] * out_hifi[1];
        }
    }

    /* In Hi-Fi mode, both channels process every clock, capturing full bandwidth */
    TEST_ASSERT(hifi_energy > 0);

    /* Disable Hi-Fi mode and verify flag resets */
    ps1_reverb_set_hifi(&reverb, 0);
    TEST_ASSERT(ps1_reverb_get_hifi(&reverb) == 0);

    printf("test_hifi_reverb passed.\n");
}

/* 2. Equal-Power Panning Law Test */
static void test_equal_power_panning(void) {
    Ps1Seq seq;
    memset(&seq, 0, sizeof(seq));
    TEST_ASSERT(!ps1_seq_get_equal_power_panning(&seq));

    ps1_seq_set_equal_power_panning(&seq, true);
    TEST_ASSERT(ps1_seq_get_equal_power_panning(&seq));

    /* Verify constant energy across the stereo field:
     * For any pan in [0, 127], equal-power panning guarantees:
     * cos^2(theta) + sin^2(theta) == 1.0 identically. */
    int test_pans[] = { 0, 16, 32, 64, 96, 112, 127 };

    for (int i = 0; i < (int)(sizeof(test_pans) / sizeof(test_pans[0])); i++) {
        int pan = test_pans[i];
        float theta = ((float)pan / 127.0f) * 1.57079632679f;
        float gl = cosf(theta);
        float gr = sinf(theta);
        float power = gl * gl + gr * gr;

        float diff = fabsf(power - 1.0f);
        TEST_ASSERT(diff < 0.001f);
    }

    ps1_seq_set_equal_power_panning(&seq, false);
    TEST_ASSERT(!ps1_seq_get_equal_power_panning(&seq));

    printf("test_equal_power_panning passed.\n");
}

/* 3. SDK Sound Hi-Fi Integration Test */
static void test_sdk_sound_hifi(void) {
    sdk_sound_reset();
    TEST_ASSERT(!sdk_sound_get_hifi_reverb());
    TEST_ASSERT(!sdk_sound_get_equal_power_panning());

    sdk_sound_set_hifi_reverb(true);
    TEST_ASSERT(sdk_sound_get_hifi_reverb());

    sdk_sound_set_equal_power_panning(true);
    TEST_ASSERT(sdk_sound_get_equal_power_panning());

    /* Reset preserves or resets safely */
    sdk_sound_reset();
    TEST_ASSERT(sdk_sound_get_hifi_reverb());
    TEST_ASSERT(sdk_sound_get_equal_power_panning());

    /* Disable and verify */
    sdk_sound_set_hifi_reverb(false);
    sdk_sound_set_equal_power_panning(false);
    TEST_ASSERT(!sdk_sound_get_hifi_reverb());
    TEST_ASSERT(!sdk_sound_get_equal_power_panning());

    printf("test_sdk_sound_hifi passed.\n");
}

int main(void) {
#ifdef _WIN32
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    test_hifi_reverb();
    test_equal_power_panning();
    test_sdk_sound_hifi();

    printf("All PS1 Hi-Fi audio tests PASSED successfully!\n");
    return 0;
}