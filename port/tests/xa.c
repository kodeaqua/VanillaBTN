#include "psx/cd_runtime.h"
#include "psx/sound_voice.h"
#include "psx/xa_decode.h"
#include <libcd.h>
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
static void hm_fixture(u8 raw[2352], u8 channel) {
    memset(raw, 0, 2352);
    memset(raw + 1, 255, 10);
    raw[15] = 2;
    raw[16] = 1;
    raw[17] = channel;
    raw[18] = 0xE4;
    raw[19] = 1;
    memcpy(raw + 20, raw + 16, 4);
    raw[24 + 16] = 0x21;
}
static int hm_read(void *context, u64 offset, void *destination, size_t size) {
    (void)context;
    if (offset % 2352 || size != 2352 || offset / 2352 > 1)
        return 0;
    hm_fixture(destination, offset == 0 ? 1 : 0);
    return 1;
}
int main(void) {
    hm_xa_decoder decoder;
    u8 raw[2352];
    s16 expected[HM_XA_FRAMES * 2], actual[HM_XA_FRAMES * 2];
    hm_fixture(raw, 0);
    hm_xa_decoder_reset(&decoder);
    CHECK(hm_xa_decode_sector(&decoder, raw, expected));
    const s16 first[14] = {-1, -1, 0, 0, -1, -2, 0, 1, 1, 2, 3, 6, -9, -17};
    CHECK(!memcmp(expected, first, sizeof(first)));
    CHECK(decoder.samples == 0 && decoder.position == 0);
    hm_xa_decoder_reset(&decoder);
    CHECK(hm_xa_decode_sector(&decoder, raw, actual));
    CHECK(!memcmp(expected, actual, sizeof(actual)));
    raw[19] = raw[23] = 0; /* Unsupported coding, not a silent mono substitute. */
    CHECK(!hm_xa_decode_sector(&decoder, raw, actual));
    hm_fixture(raw, 0);
    raw[24 + 16] = 0x21;
    for (u32 group = 0; group < 18; ++group)
        memset(raw + 24 + group * 128 + 4, 0x1C, 8);
    hm_xa_decoder_reset(&decoder);
    CHECK(hm_xa_decode_sector(&decoder, raw, actual));
    CHECK(decoder.history[0].previous == 1 && decoder.history[1].previous == 2);
    CHECK(hm_xa_decode_sector(&decoder, raw, actual));
    CHECK(decoder.history[0].previous > 0 && decoder.history[1].previous > 0);

    hm_disc disc = {.stream = {NULL, 2 * 2352, hm_read}, .sectors = 2, .mounted = 1};
    hm_cd_attach(&disc);
    CHECK(CdInit() == 1);
    const u8 filter[2] = {1, 0}, mode = 0xC8;
    CHECK(CdControlB(CdlSetfilter, filter, NULL) == 1);
    CHECK(CdControlB(CdlSetmode, &mode, NULL) == 1);
    CdlLOC position;
    CdIntToPos(0, &position);
    CHECK(CdControl(CdlReadS, (const u8 *)&position, NULL) == 1);
    hm_cd_tick(); /* Ignore other channel's EOF; decode selected channel's EOF. */
    struct {
        u8 response[8];
        u32 sentinel;
    } response = {{0}, 0x12345678};
    CHECK(CdControlF(CdlGetlocP, NULL) == 1);
    CHECK(CdSync(1, response.response) == 2);
    CHECK(response.sentinel == 0x12345678 && response.response[0] == 1);
    CHECK(CdPosToInt((const CdlLOC *)(response.response + 5)) == 2);
    SsInit();
    SsSetMVol(127, 127);
    SsSetSerialAttr(0, 0, 1);
    SsSetSerialVol(0, 127, 127);
    hm_sound_mix(actual, HM_XA_FRAMES);
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    s16 empty[2];
    CHECK(!hm_cd_audio_sample(empty) && !empty[0] && !empty[1]);
    CHECK(CdControl(CdlReadS, (const u8 *)&position, NULL) == 1);
    hm_cd_tick();
    SsSetSerialVol(0, 0, 0);
    hm_sound_mix(actual, HM_XA_FRAMES);
    for (u32 i = 0; i < HM_XA_FRAMES * 2; ++i)
        CHECK(!actual[i]);
    CHECK(!hm_cd_audio_sample(empty));
    CHECK(CdControl(CdlReadS, (const u8 *)&position, NULL) == 1);
    hm_cd_tick();
    CHECK(CdControl(CdlStandby, NULL, NULL) == 1 && !hm_cd_audio_sample(empty));
    hm_cd_detach();
    return 0;
}
