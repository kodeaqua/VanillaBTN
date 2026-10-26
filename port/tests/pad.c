#include "psx/pad.h"
#include <libpad.h>
#include <stdio.h>
#include <string.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    u8 first[8], second[8], actuators[8] = {0};
    PadInitDirect(first, second);
    PadSetAct(0, actuators, 8);
    PadSetAct(0x10, actuators, 8);
    PadStartCom();
    CHECK(PadGetState(0) == PadStateDiscon && first[0] == 0xFF);
    hm_pad_sample(0, 1, 0x1040); /* Up and Cross in game bit order. */
    CHECK(PadGetState(0) == PadStateStable && PadInfoMode(0, InfoModeCurExID, 0) == 4);
    CHECK(first[0] == 0 && first[1] == 0x41 && first[2] == 0xEF && first[3] == 0xBF);
    memset(first, 0, sizeof(first)); /* The source clears its buffers after startup. */
    CHECK(PadGetState(0) == PadStateStable && first[1] == 0x41 && first[2] == 0xEF);
    hm_pad_sample(0x10, 1, 0x0080);
    CHECK(second[3] == 0x7F && first[3] == 0xBF);
    hm_pad_sample(0, 0, 0xFFFF);
    CHECK(PadGetState(0) == PadStateDiscon && first[0] == 0xFF && first[2] == 0xFF);
    CHECK(PadSetActAlign(0, actuators) == 0);
    return 0;
}
