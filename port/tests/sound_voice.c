#include "psx/sound_voice.h"
#include "psx/sequence.h"
#include <libsnd.h>
#include <libspu.h>
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
    CHECK(hm_sound_note_pitch(60, 0, 60, 0) == 4096);
    CHECK(hm_sound_note_pitch(72, 0, 60, 0) == 8192);
    CHECK(hm_sound_note_pitch(48, 0, 60, 0) == 2048);
    CHECK(hm_sound_note_pitch(60, 127, 60, 0) == hm_sound_note_pitch(61, 0, 60, 0));
    CHECK(hm_sound_note_pitch(127, 127, 0, 0) == 0x4000);
    u8 header[0xC20] = {0};
    memcpy(header, "pBAV", 4);
    header[4] = 7;
    header[12] = 0x40;
    header[13] = 0x0C;
    header[18] = header[22] = 1;
    header[24] = 127;
    header[25] = 64;
    header[32 + 7 * 16] = 1;
    header[32 + 7 * 16 + 1] = 127;
    header[32 + 7 * 16 + 4] = 64;
    header[0x820 + 2] = 127;
    header[0x820 + 3] = 64;
    header[0x820 + 4] = 60;
    header[0x820 + 7] = 127;
    header[0x820 + 16] = 15;
    header[0x820 + 18] = 0xC0;
    header[0x820 + 19] = 0x1F;
    header[0x820 + 22] = 1;
    header[0xA22] = 4;
    u8 sample[32];
    memset(sample, 0x77, sizeof(sample));
    sample[0] = sample[16] = 4;
    sample[1] = 4;
    sample[17] = 3;
    SsInit();
    SsSetMVol(127, 127);
    CHECK(SsVabOpenHead(header, -1) == 0);
    CHECK(SsVabTransBodyPartly(sample, sizeof(sample), 0) == 0);
    CHECK(SsUtKeyOnV(23, 0, 7, 0, 60, 0, 127, 0) == 23);
    CHECK(SpuGetKeyStatus(1u << 23) == 3);
    s16 output[2048];
    hm_sound_mix(output, 1024);
    int audible = 0;
    for (u32 i = 0; i < 1024; ++i) {
        audible |= output[i * 2] != 0;
        CHECK(!output[i * 2 + 1]);
    }
    CHECK(audible && SpuGetKeyStatus(1u << 23) == 1);
    CHECK(hm_sound_bank_has_voices(0));
    CHECK(SsUtKeyOffV(23) == 0 && SpuGetKeyStatus(1u << 23) == 2);
    hm_sound_mix(output, 1024);
    CHECK(!hm_sound_bank_has_voices(0) && SpuGetKeyStatus(1u << 23) == 0);
    /* 60 MIDI ticks/quarter, 0.5 seconds/quarter: a 30-tick note lasts
     * exactly 15 VBlanks. The song selects sparse program seven. */
    u8 seq[] = {'p', 'Q',  'E', 'S', 0,    0,  0,   1,  0,    60, 7, 0xA1, 0x20, 4,    2,
                0,   0xC0, 7,   0,   0x90, 60, 100, 30, 0x80, 60, 0, 0,    0xFF, 0x2F, 0};
    hm_sound_sequence_register(seq, sizeof(seq));
    u32 table[16];
    SsSetTableSize(table, 2, 1);
    SsSeqSetVol(1, 0, 0); /* The original loader mutes unopened slots too. */
    SsSetTickMode(1);
    SsSetReservedVoice(22);
    SsStart();
    CHECK(SsSeqOpen((const u32 *)seq, 0) == 0);
    SsSeqPlay(0, 1, 1);
    for (u32 tick = 0; tick < 14; ++tick) {
        hm_sound_tick();
        CHECK(SpuGetKeyStatus(1) == 1);
    }
    hm_sound_tick();
    CHECK(SpuGetKeyStatus(1) == 0);
    SsSeqClose(0);
    u8 loop_seq[] = {'p', 'Q', 'E',  'S',  0,  0,    0,  1,    0,  60,   7,  0xA1, 0x20, 4,
                     2,   0,   0xC0, 7,    0,  0xB0, 99, 20,   0,  0xB0, 6,  1,    0,    0x90,
                     60,  100, 30,   0x80, 60, 0,    0,  0xB0, 99, 30,   30, 0xFF, 0x2F, 0};
    hm_sound_sequence_register(loop_seq, sizeof(loop_seq));
    CHECK(SsSeqOpen((const u32 *)loop_seq, 0) == 0);
    SsSeqPlay(0, 1, 1);
    for (u32 tick = 0; tick < 29; ++tick) {
        hm_sound_tick();
        CHECK(SpuGetKeyStatus(1) == 1);
    }
    hm_sound_tick();
    CHECK(SpuGetKeyStatus(1) == 0);
    SsSeqClose(0);
    CHECK(SsUtKeyOnV(23, 0, 7, 0, 60, 0, 127, 0) == 23);
    hm_sound_mix(output, 1024);
    SsVabClose(0);
    hm_sound_mix(output, 1024); /* Retired body remains alive while referenced. */
    CHECK(SpuGetKeyStatus(1u << 23) == 1);
    SsEnd();
    SsUtKeyOffV(23);
    hm_sound_tick(); /* Sequencer shutdown does not stop the SPU release tail. */
    CHECK(SpuGetKeyStatus(1u << 23) == 0);
    SsInit();
    return 0;
}
