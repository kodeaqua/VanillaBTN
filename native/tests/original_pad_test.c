#include "sdk_pad.h"
#include "sdk_sync.h"
#include "farm_state.h"
#include "main.h"
#include <libpad.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

/* Actual original types, supplied as main-loop state fixtures. No controller
 * repeat, analog, replay or vibration interpretation is duplicated here. */
FarmState game_state;
InputRecord input_records[2];
InputRecord *input_pad1 = input_records, *input_pad2 = input_records + 1;
u8 main_get_speed_mode(void) { return MAIN_SPEED_FAST_2; }
static uint64_t clock_time;
static uint64_t now(void *context) { (void)context; return clock_time; }
static void wait_until(uint64_t deadline, void *context) { (void)context; clock_time = deadline; }
static unsigned small_motor, large_motor;
static void rumble(unsigned slot, uint8_t small, uint8_t large, void *context) {
    (void)context;
    if (slot == 0) { small_motor = small; large_motor = large; }
}

int main(void) {
    CHECK(sdk_sync_bind(now, wait_until, NULL));
    sdk_pad_rumble(rumble, NULL);
    sdk_pad_sample(0, true, false, PAD_CROSS, 128, 128);
    sdk_pad_sample(1, true, false, PAD_TRIANGLE, 128, 128);
    pad_init();
    pad_reset_records();
    pad_poll_ports();
    CHECK(input_pad1->held == PAD_CROSS && input_pad1->pressed == PAD_CROSS);
    CHECK(input_pad1->triggered == PAD_CROSS && input_pad1->analog == -1);
    CHECK(input_pad2->held == PAD_TRIANGLE && input_pad2->pressed == PAD_TRIANGLE);
    for (unsigned i = 1; i <= 14; ++i) {
        pad_poll_ports();
        CHECK(input_pad1->pressed == 0);
        CHECK(input_pad1->triggered == (i == 14 ? PAD_CROSS : 0));
        CHECK(input_pad1->triggered2 == input_pad1->triggered);
    }
    sdk_pad_sample(0, false, false, 0, 128, 128);
    pad_poll_ports();
    CHECK(input_pad1->held == 0 && input_pad1->analog == -1);
    sdk_pad_sample(0, true, true, 0, 255, 128);
    pad_poll_ports();
    CHECK(input_pad1->held == 0);
    pad_poll_ports();
    CHECK(PadGetState(0) == PadStateExecCmd && input_pad1->held == 0);
    clock_time = 50000000; /* Three nominal vblanks complete actuator setup. */
    pad_poll_ports();
    CHECK(PadGetState(0) == PadStateStable);
    CHECK(input_pad1->held == PAD_RIGHT && input_pad1->analog == 90);
    sdk_pad_sample(0, true, true, 0, 164, 164);
    pad_poll_ports();
    CHECK(input_pad1->held == 0 && input_pad1->analog == 0);
    sdk_pad_sample(0, true, true, 0, 255, 255);
    pad_poll_ports();
    CHECK(input_pad1->held == 0); /* Original exact 45-degree sector boundary. */
    sdk_pad_sample(0, true, true, PAD_LEFT, 255, 128);
    pad_poll_ports();
    CHECK(input_pad1->held == PAD_LEFT && input_pad1->analog == -1);
    sdk_pad_sample(0, true, false, PAD_CROSS, 128, 128);
    pad_poll_ports();
    pad_poll_ports();
    CHECK(input_pad1->held == PAD_CROSS && input_pad1->analog == -1);
    sdk_pad_sample(0, true, true, 0, 255, 128);
    pad_poll_ports();
    pad_poll_ports();
    CHECK(PadGetState(0) == PadStateExecCmd);
    clock_time = 100000000;
    pad_poll_ports();
    CHECK(input_pad1->held == PAD_RIGHT);
    input_pad1->actuator0 = 1;
    input_pad1->actuator1 = 200;
    pad_poll_ports();
    PadGetState(0);
    CHECK(small_motor == 1 && large_motor == 200);
    InputDemo demo = {0};
    game_state.demo = &demo;
    game_state.globalFlags = GLOBAL_FLAG_INPUT_REPLAY;
    demo.buttons = PAD_CIRCLE;
    sdk_pad_sample(0, false, true, 0, 128, 128);
    pad_poll_ports();
    CHECK(input_pad1->held == PAD_CROSS && input_pad1->analog == 0);
    PadStopCom();
    CHECK(small_motor == 0 && large_motor == 0);
    sdk_pad_rumble(NULL, NULL);
    CHECK(sdk_sync_bind(NULL, NULL, NULL));
    return 0;
}
