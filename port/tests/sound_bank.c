#include "psx/sound_bank.h"
#include <libsnd.h>
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
    u8 header[0xC20] = {0};
    memcpy(header, "pBAV", 4);
    header[4] = 7;
    header[12] = 0x40;
    header[13] = 0x0C;
    header[18] = 1;
    header[22] = 1;
    header[32 + 16 * 7] = 1; /* Sparse program seven, packed tone block zero. */
    header[0xA22] = 4;       /* Sample one occupies 32 body bytes; sample zero is empty. */
    header[0x820 + 22] = 1;  /* Program seven's first packed tone selects sample one. */
    SsInit();
    s32 bank = SsVabOpenHead(header, -1);
    CHECK(bank == 0 && hm_sound_bank_get(bank)->programs == 1);
    CHECK(hm_sound_bank_get(bank)->header_bytes == sizeof(header));
    CHECK(hm_sound_bank_get(bank)->body_bytes == 32);
    u8 chunk[24];
    memset(chunk, 0x12, sizeof(chunk));
    CHECK(SsVabTransBodyPartly(chunk, sizeof(chunk), bank) == 8);
    CHECK(SsVabTransCompleted(0) == 1);
    memset(chunk, 0x34, sizeof(chunk));
    CHECK(SsVabTransBodyPartly(chunk, sizeof(chunk), bank) == 0);
    const hm_sound_bank *loaded = hm_sound_bank_get(bank);
    CHECK(loaded->uploaded == 32 && loaded->body[23] == 0x12 && loaded->body[24] == 0x34);
    CHECK(loaded->body[31] == 0x34 && loaded->header[32 + 16 * 7] == 1);
    const u8 *tone = hm_sound_bank_tone(bank, 7, 0);
    CHECK(tone && tone[22] == 1 && !hm_sound_bank_tone(bank, 6, 0));
    CHECK(!hm_sound_bank_tone(bank, 7, 1));
    u32 sample_bytes;
    CHECK(hm_sound_bank_sample(bank, 1, &sample_bytes) == loaded->body && sample_bytes == 32);
    CHECK(!hm_sound_bank_sample(bank, 0, &sample_bytes));
    CHECK(SsVabOpenHead(header, bank) == -1);
    CHECK(SsVabOpenHead(header, -1) == 1);
    SsVabClose(bank);
    CHECK(!hm_sound_bank_get(bank));
    CHECK(SsVabOpenHead(header, -1) == bank);
    SsInit();
    CHECK(!hm_sound_bank_get(0) && !hm_sound_bank_get(1));
    return 0;
}
