#include "psx/gte_core.h"
#include <stdio.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    /* Literal vectors from the public GTE formulas, including FLAGS. */
    hm_gte_state state = {.ir = {-3, 4, -12}};
    hm_gte_command(&state, 0x4AA00428);
    CHECK(state.mac[1] == 9 && state.mac[2] == 16 && state.mac[3] == 144);
    CHECK(state.ir[0] == 9 && state.ir[2] == 144 && state.flags == 0);
    state.ir[0] = 32767;
    state.ir[1] = -32768;
    state.ir[2] = 182;
    hm_gte_command(&state, 0x4AA00428);
    CHECK(state.mac[1] == 1073676289 && state.mac[2] == 1073741824 && state.mac[3] == 33124);
    CHECK(state.ir[0] == 32767 && state.ir[1] == 32767 && state.ir[2] == 32767);
    CHECK(state.flags == 0x81C00000);
    state.ir[0] = 0;
    state.ir[1] = 0;
    state.ir[2] = 182;
    hm_gte_command(&state, 0x4AA00428);
    CHECK(state.flags == 0x00400000); /* IR3 alone does not set summary bit. */
    state.ir[0] = 4096;
    state.ir[1] = -4096;
    state.ir[2] = 128;
    hm_gte_command(&state, 0x4AA80428);
    CHECK(state.mac[1] == 4096 && state.mac[2] == 4096 && state.mac[3] == 4 && state.flags == 0);

    state = (hm_gte_state){.sx = {0, 10, 0}, .sy = {0, 0, 10}};
    hm_gte_command(&state, 0x4B400006);
    CHECK(state.mac[0] == 100 && !state.flags);
    state.sx[1] = -10;
    hm_gte_command(&state, 0x4B400006);
    CHECK(state.mac[0] == -100 && !state.flags);
    state = (hm_gte_state){.sx = {-32768, 32767, -32768}, .sy = {-32768, -32768, 32767}};
    hm_gte_command(&state, 0x4B400006);
    CHECK(state.mac[0] == -131071 && state.flags == 0x80010000);

    state = (hm_gte_state){.sz = {100, 200, 300, 400}, .zsf = {1365, 1024}};
    hm_gte_command(&state, 0x4B58002D);
    CHECK(state.mac[0] == 1228500 && state.otz == 299 && !state.flags);
    hm_gte_command(&state, 0x4B68002E);
    CHECK(state.mac[0] == 1024000 && state.otz == 250 && !state.flags);
    state.zsf[1] = -1;
    hm_gte_command(&state, 0x4B68002E);
    CHECK(state.mac[0] == -1000 && state.otz == 0 && state.flags == 0x80040000);
    for (u32 i = 0; i < 4; ++i)
        state.sz[i] = 65535;
    state.zsf[1] = 32767;
    hm_gte_command(&state, 0x4B68002E);
    CHECK(state.mac[0] == -393212 && state.otz == 65535 && state.flags == 0x80050000);

    state = (hm_gte_state){.ir = {4, 5, 6}};
    for (u32 i = 0; i < 3; ++i)
        state.matrix[0][i][i] = (s16)(i + 1);
    hm_gte_command(&state, 0x4B70000C);
    CHECK(state.mac[1] == -3 && state.mac[2] == 6 && state.mac[3] == -3 && !state.flags);

    state = (hm_gte_state){.vector = {{123, -456, 789}}, .translation = {{2, -3, 4}}};
    for (u32 i = 0; i < 3; ++i)
        state.matrix[0][i][i] = 4096;
    hm_gte_command(&state, 0x4A480012);
    CHECK(state.mac[1] == 125 && state.mac[2] == -459 && state.mac[3] == 793 && !state.flags);
    state.ir[0] = -1;
    state.ir[1] = 5;
    state.ir[2] = -9;
    hm_gte_command(&state, 0x4A49E012);
    CHECK(state.mac[1] == -1 && state.mac[2] == 5 && state.mac[3] == -9 && !state.flags);
    hm_gte_command(&state, 0x4A49E412);
    CHECK(state.ir[0] == 0 && state.ir[1] == 5 && state.ir[2] == 0 && state.flags == 0x81400000);
    CHECK(hm_gte_shift(-1, 12) == -1 && hm_gte_shift(-4097, 12) == -2);
    state = (hm_gte_state){.vector = {{32767, 0, 0}}, .translation = {{INT32_MAX, 0, 0}}};
    state.matrix[0][0][0] = 32767;
    hm_gte_command(&state, 0x4A480012);
    CHECK(state.mac[1] == -2147221521 && state.ir[0] == -32768 && state.flags == 0xC1000000);
    state = (hm_gte_state){0};
    CHECK(hm_gte_accumulate(&state, 1, -(INT64_C(1) << 43) - 1) == (INT64_C(1) << 43) - 1);
    CHECK(state.flags == 0x04000000);
    state = (hm_gte_state){0};
    CHECK(hm_gte_accumulate(&state, 2, (INT64_C(1) << 43) - 1) == (INT64_C(1) << 43) - 1);
    CHECK(hm_gte_accumulate(&state, 2, -(INT64_C(1) << 43)) == -(INT64_C(1) << 43));
    CHECK(!state.flags);
    return 0;
}
