#include "psx/sound_bank.h"
#include "compat/asset.h"
#include "psx/error.h"
#include "psx/sound_control.h"
#include <libsnd.h>
#include <stdlib.h>
#include <string.h>

/* Native assets, not a mapped SPU RAM image. Offsets remain bank-relative. */
static hm_sound_bank hm_banks[16];
static s32 hm_transfer_bank = -1;
static u32 hm_u16le(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8); }

const hm_sound_bank *hm_sound_bank_get(s32 bank) {
    return bank >= 0 && bank < 16 && hm_banks[bank].header ? hm_banks + bank : NULL;
}

void SsVabClose(s32 bank) {
    if (!hm_sound_bank_get(bank))
        hm_runtime_error("SsVabClose", "invalid bank");
    free(hm_banks[bank].header);
    free(hm_banks[bank].body);
    memset(hm_banks + bank, 0, sizeof(hm_banks[bank]));
    if (hm_transfer_bank == bank)
        hm_transfer_bank = -1;
}

void hm_sound_banks_reset(void) {
    for (s32 bank = 0; bank < 16; ++bank)
        if (hm_banks[bank].header)
            SsVabClose(bank);
    hm_transfer_bank = -1;
}

s32 SsVabOpenHead(const void *header, s32 bank) {
    if (!hm_sound_configuration()->initialized || !header)
        hm_runtime_error("SsVabOpenHead", "uninitialized sound or null header");
    const u8 *p = header;
    if (memcmp(p, "pBAV", 4) || hm_asset_s32le(p + 4) != 7)
        hm_runtime_error("SsVabOpenHead", "unsupported VAB signature or version");
    u32 programs = hm_u16le(p + 18), samples = hm_u16le(p + 22);
    if (programs > 128 || samples > 254)
        hm_runtime_error("SsVabOpenHead", "invalid VAB counts");
    /* Tone blocks are packed across populated program slots. */
    u32 populated = 0;
    for (u32 i = 0; i < 128; ++i) {
        u32 tones = p[32 + i * 16];
        if (tones > 16)
            hm_runtime_error("SsVabOpenHead", "invalid tone count");
        populated += tones != 0;
    }
    if (populated != programs)
        hm_runtime_error("SsVabOpenHead", "inconsistent program count");
    u32 header_bytes = 0xA20 + programs * 512;
    s32 total = hm_asset_s32le(p + 12);
    if (total < (s32)header_bytes || (u32)total - header_bytes > 0x80000)
        hm_runtime_error("SsVabOpenHead", "invalid VAB data size");
    if (bank == -1) {
        for (bank = 0; bank < 16 && hm_banks[bank].header; ++bank) {
        }
        if (bank == 16)
            return -1;
    }
    if (bank < 0 || bank >= 16 || hm_banks[bank].header)
        return -1;
    u32 body_bytes = (u32)total - header_bytes;
    u8 *native_header = malloc(header_bytes);
    u8 *body = calloc(body_bytes ? body_bytes : 1, 1);
    if (!native_header || !body) {
        free(native_header);
        free(body);
        return -1;
    }
    memcpy(native_header, header, header_bytes);
    hm_banks[bank] = (hm_sound_bank){native_header, body,        header_bytes, body_bytes, 0,
                                     (u16)programs, (u16)samples};
    hm_transfer_bank = bank;
    return bank;
}

s32 SsVabTransBodyPartly(const void *data, s32 bytes, s32 bank) {
    if (!hm_sound_bank_get(bank) || !data || bytes <= 0)
        hm_runtime_error("SsVabTransBodyPartly", "invalid transfer");
    hm_sound_bank *target = hm_banks + bank;
    u32 remaining = target->body_bytes - target->uploaded;
    /* Final fixed-sized disc chunks include archive padding. */
    u32 copied = (u32)bytes < remaining ? (u32)bytes : remaining;
    memcpy(target->body + target->uploaded, data, copied);
    target->uploaded += copied;
    hm_transfer_bank = bank;
    return (s32)(target->body_bytes - target->uploaded);
}

s32 SsVabTransCompleted(s32 mode) {
    if (mode != 0 && mode != 1)
        hm_runtime_error("SsVabTransCompleted", "invalid synchronization mode");
    if (hm_transfer_bank < 0)
        hm_runtime_error("SsVabTransCompleted", "no bank transfer");
    /* Each chunk is copied synchronously, independently of future disc chunks.
     * This does not acknowledge sample playback or a working mixer. */
    return 1;
}
