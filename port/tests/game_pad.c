#include "../../src/main/input/pad.c"
#include "psx/pad.h"
#include <stdio.h>

FarmState game_state;
InputRecord input_records[2];
InputRecord *input_pad1 = &input_records[0];
InputRecord *input_pad2 = &input_records[1];
u8 main_get_speed_mode(void) { return 0; }
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    pad_init();
    pad_reset_records();
    hm_pad_sample(0, 1, 0);
    hm_pad_sample(0x10, 0, 0);
    pad_poll_ports();
    CHECK(!input_pad1->held && !input_pad2->held);
    hm_pad_sample(0, 1, PAD_START);
    pad_poll_ports();
    CHECK(input_pad1->held == PAD_START && input_pad1->pressed == PAD_START);
    pad_poll_ports();
    CHECK(input_pad1->held == PAD_START && !input_pad1->pressed);
    hm_pad_sample(0, 1, 0);
    pad_poll_ports();
    CHECK(!input_pad1->held && !input_pad1->pressed);
    hm_pad_sample(0, 1, PAD_UP | PAD_CROSS);
    pad_poll_ports();
    CHECK(input_pad1->pressed == (PAD_UP | PAD_CROSS));
    pad_reset_records();
    pad_poll_ports();
    CHECK(input_pad1->held == (PAD_UP | PAD_CROSS));
    return 0;
}
