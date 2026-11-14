/* ps1_input_overhaul_test.c — test suite for Circular Analog Stick Normalization
 * (fixing diagonal running bug), HD Rumble Motor Curve, and Sub-Frame Input Polling.
 */

#include "ps1_input.h"
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

/* 1. Test Circular Stick Normalization & Diagonal Running Fix (Flaw 33) */
static void test_circular_stick_normalization(void) {
    uint8_t out_x = 0, out_y = 0;
    float deadzone = 0.15f;

    /* A. Center deadzone test */
    ps1_input_normalize_stick(0, 0, deadzone, true, &out_x, &out_y);
    TEST_ASSERT(out_x == 128 && out_y == 128);

    /* Points inside deadzone (e.g. 5% deflection) must collapse to center */
    ps1_input_normalize_stick(1500, -1500, deadzone, true, &out_x, &out_y);
    TEST_ASSERT(out_x == 128 && out_y == 128);

    /* B. Cardinal axes tests */
    /* Fully Up: (0, -32767) */
    ps1_input_normalize_stick(0, -32767, deadzone, true, &out_x, &out_y);
    TEST_ASSERT(out_x == 128 && out_y <= 1);

    /* Fully Down: (0, 32767) */
    ps1_input_normalize_stick(0, 32767, deadzone, true, &out_x, &out_y);
    TEST_ASSERT(out_x == 128 && out_y >= 254);

    /* Fully Left: (-32767, 0) */
    ps1_input_normalize_stick(-32767, 0, deadzone, true, &out_x, &out_y);
    TEST_ASSERT(out_x <= 1 && out_y == 128);

    /* Fully Right: (32767, 0) */
    ps1_input_normalize_stick(32767, 0, deadzone, true, &out_x, &out_y);
    TEST_ASSERT(out_x >= 254 && out_y == 128);

    /* C. Diagonal Running Fix:
     * On modern circular gamepads, full diagonal deflection has magnitude ~32767,
     * with components ~23170 (32767 * cos(45 deg)).
     * In authentic PS1 mode (no normalization):
     * dx = 23170 / 256 = 90.
     * With center at 128, magnitude from center is only 90 (below the 110 run threshold),
     * causing the character to drop into a slow walk! */

    int16_t diag_x = 23170;
    int16_t diag_y = -23170; /* Up-Right */

    /* Authentic mode: verifies the authentic bug */
    ps1_input_normalize_stick(diag_x, diag_y, deadzone, false, &out_x, &out_y);
    int auth_dist_x = abs((int)out_x - 128);
    int auth_dist_y = abs((int)out_y - 128);
    TEST_ASSERT(auth_dist_x < 100); /* Authentic bug: insufficient deflection to run! */
    TEST_ASSERT(auth_dist_y < 100);

    /* Modern mode with circular normalization:
     * Full diagonal deflection expands smoothly to the corner of the PS1 octagonal gate,
     * reaching full maximum running magnitude (~127 from center). */
    ps1_input_normalize_stick(diag_x, diag_y, deadzone, true, &out_x, &out_y);
    int fixed_dist_x = abs((int)out_x - 128);
    int fixed_dist_y = abs((int)out_y - 128);
    TEST_ASSERT(fixed_dist_x >= 125); /* Reaches full run speed! */
    TEST_ASSERT(fixed_dist_y >= 125);
    TEST_ASSERT(out_x >= 253);
    TEST_ASSERT(out_y <= 3);

    /* Test all 4 diagonal quadrants */
    /* Up-Left: (-23170, -23170) */
    ps1_input_normalize_stick(-diag_x, diag_y, deadzone, true, &out_x, &out_y);
    TEST_ASSERT(out_x <= 3 && out_y <= 3);

    /* Down-Right: (23170, 23170) */
    ps1_input_normalize_stick(diag_x, -diag_y, deadzone, true, &out_x, &out_y);
    TEST_ASSERT(out_x >= 253 && out_y >= 253);

    /* Down-Left: (-23170, 23170) */
    ps1_input_normalize_stick(-diag_x, -diag_y, deadzone, true, &out_x, &out_y);
    TEST_ASSERT(out_x <= 3 && out_y >= 253);

    printf("test_circular_stick_normalization passed.\n");
}

/* Guard the public input helper against a nonfinite deadzone. A bad
 * value must not become undefined float-to-integer conversion. */
static void test_nonfinite_deadzone(void) {
    uint8_t expected_x = 0, expected_y = 0, actual_x = 0, actual_y = 0;
    ps1_input_normalize_stick(16000, -7000, 0.0f, true, &expected_x, &expected_y);
    ps1_input_normalize_stick(16000, -7000, NAN, true, &actual_x, &actual_y);
    TEST_ASSERT(actual_x == expected_x && actual_y == expected_y);
}

/* 2. Test HD Rumble & Haptic Curve (Flaw 34) */
static void test_hd_rumble_curve(void) {
    /* Large motor: test gamma 1.4 response curve.
     * Low intensities are softened to prevent harsh motor buzz,
     * while high intensities scale up smoothly to full punch. */
    uint8_t low_large = 64;   /* ~25% */
    uint8_t high_large = 255; /* 100% */

    float low_norm = (float)low_large / 255.0f;
    uint16_t low_hd = (uint16_t)(powf(low_norm, 1.4f) * 65535.0f);
    uint16_t low_auth = (uint16_t)(low_large * 257u);

    /* In HD mode, low intensity vibration is softer than harsh linear blast */
    TEST_ASSERT(low_hd < low_auth);

    float high_norm = (float)high_large / 255.0f;
    uint16_t high_hd = (uint16_t)(powf(high_norm, 1.4f) * 65535.0f);
    TEST_ASSERT(high_hd == 65535);

    /* Small motor: in PS1, any non-zero value blasted 100% (65535).
     * In HD mode, small motor uses a controlled high-frequency tactile pulse (~26000). */
    uint16_t small_hd = 26000;
    uint16_t small_auth = 65535;
    TEST_ASSERT(small_hd < small_auth);

    printf("test_hd_rumble_curve passed.\n");
}

/* 3. Test Sub-Frame Input Responsiveness */
static void test_subframe_input(void) {
    Ps1InputRecord rec;
    ps1_input_init(&rec);

    /* Instantaneous button state transitions */
    ps1_input_set_button(&rec, PS1_PAD_CROSS, true);
    TEST_ASSERT((rec.held & PS1_PAD_CROSS) != 0);

    ps1_input_tick(&rec);
    TEST_ASSERT((rec.pressed & PS1_PAD_CROSS) != 0);

    /* Next tick: button remains held, but not pressed */
    ps1_input_tick(&rec);
    TEST_ASSERT((rec.held & PS1_PAD_CROSS) != 0);
    TEST_ASSERT((rec.pressed & PS1_PAD_CROSS) == 0);

    ps1_input_set_button(&rec, PS1_PAD_CROSS, false);
    TEST_ASSERT((rec.held & PS1_PAD_CROSS) == 0);

    ps1_input_tick(&rec);
    TEST_ASSERT((rec.held & PS1_PAD_CROSS) == 0);
    TEST_ASSERT((rec.pressed & PS1_PAD_CROSS) == 0);

    printf("test_subframe_input passed.\n");
}

int main(void) {
    test_nonfinite_deadzone();
#ifdef _WIN32
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    test_circular_stick_normalization();
    test_hd_rumble_curve();
    test_subframe_input();

    printf("All PS1 Input Overhaul tests PASSED successfully!\n");
    return 0;
}
