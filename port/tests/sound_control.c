#include "psx/sound_control.h"
#include <libsnd.h>
#include <stdio.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    char table[1056];
    SsInit();
    SsSetTableSize(table, 6, 1);
    SsSetTickMode(1);
    CHECK(SsSetReservedVoice(22) >= 0 && SsSetReservedVoice(25) == -1);
    SpuCommonAttr attributes = {
        .mask = 0x2C3, .mvol = {0x3FFF, 0x3FFF}, .cd = {{0x1FFF, 0x1FFF}, 1}};
    SpuSetCommonAttr(&attributes);
    const hm_sound_control *state = hm_sound_configuration();
    CHECK(state->initialized && state->reserved == 22 && state->sequences == 6 &&
          state->table == table);
    CHECK(state->common.mvol.left == 0x3FFF && state->common.cd.volume.right == 0x1FFF &&
          state->common.cd.mix == 1);
    attributes.mask = 1;
    attributes.mvol.left = 1234;
    attributes.mvol.right = 99;
    SpuSetCommonAttr(&attributes);
    CHECK(state->common.mvol.left == 1234 && state->common.mvol.right == 0x3FFF);
    SsSetSerialAttr(0, 0, 0);
    SsSetSerialVol(0, 128, 45);
    SsSetMVol(60, 127);
    CHECK(!state->common.cd.mix && state->serial_left == 128 && state->master_right == 127);
    SsSetMono();
    CHECK(state->mono);
    SsSetStereo();
    CHECK(!state->mono);
    SsUtReverbOff();
    SsStart();
    CHECK(state->started);
    u8 keys[24];
    SpuGetAllKeysStatus(keys);
    for (u32 i = 0; i < 24; ++i)
        CHECK(keys[i] == 0);
    CHECK(SpuGetKeyStatus(0xFFFFFF) == 0);
    SsUtAllKeyOff(0);
    SsEnd();
    CHECK(!state->started);
    return 0;
}
