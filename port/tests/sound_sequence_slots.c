#define SsSeqOpen hm_test_seq_open
#define SsStart hm_test_seq_start
#define SsSetStereo hm_test_stereo
#define SsSetMono hm_test_mono
#include "../../src/main/sound/sound.c"
#undef SsSeqOpen
#undef SsStart
#undef SsSetStereo
#undef SsSetMono
#include <stdio.h>
static int opens, starts, stereo, mono, bad;
static const u32 *expected[2];
static u32 ordinary[16];
s32 hm_test_seq_open(const u32 *data, s32 bank) {
    if (opens >= 2 || data != expected[opens] || bank != 3)
        bad = 1;
    return 7 + opens++;
}
void hm_test_seq_start(void) { ++starts; }
void hm_test_stereo(void) { ++stereo; }
void hm_test_mono(void) { ++mono; }
int main(void) {
    for (int i = 0; i < 6; ++i) {
        seq_data[i] = SOUND_BUFFER_NONE;
        *(s32 *)&seq_slots[i] = -1;
    }
    /* Opaque mock address: no dereference. A sentinel low word is not sufficient. */
    seq_data[0] = (u8 *)(uintptr_t)UINT64_C(0x1ffffffff);
    seq_data[1] = (u8 *)ordinary;
    expected[0] = (const u32 *)seq_data[0];
    expected[1] = ordinary;
    seq_vabs[0] = seq_vabs[1] = 3;
    sound_ss_running = 0;
    sound_stereo = 1;
    if (sound_update() != 0 || opens != 2 || starts != 1 || stereo != 1 || mono || bad)
        return 1;
    if (*(s32 *)&seq_slots[0] != 7 || *(s32 *)&seq_slots[1] != 8)
        return 1;
    sound_stereo = 0;
    if (sound_update() != 0 || opens != 2 || starts != 1 || mono != 1 || bad)
        return 1;
    return 0;
}
