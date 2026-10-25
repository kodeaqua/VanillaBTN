#include "platform/input_replay.h"
#include <stdio.h>
#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(void) {
    hm_input_replay replay = {0};
    CHECK(hm_input_replay_line(&replay, "# test transition timing"));
    CHECK(hm_input_replay_line(&replay, "10 0800\n"));
    CHECK(hm_input_replay_line(&replay, "20 0000\n"));
    CHECK(!hm_input_replay_line(&replay, "20 0040"));
    CHECK(!hm_input_replay_line(&replay, "-1 0040"));
    CHECK(!hm_input_replay_line(&replay, "30 10000"));
    CHECK(!hm_input_replay_line(&replay, "30 0040 garbage"));
    CHECK(hm_input_replay_sample(&replay, 9) == 0);
    CHECK(hm_input_replay_sample(&replay, 10) == 0x0800);
    CHECK(hm_input_replay_sample(&replay, 19) == 0x0800);
    CHECK(hm_input_replay_sample(&replay, 20) == 0);
    CHECK(hm_input_replay_sample(&replay, 999) == 0);
    return 0;
}
