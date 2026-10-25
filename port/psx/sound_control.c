#include "psx/sound_control.h"
#include "psx/error.h"
#include "psx/reverb.h"
#include "psx/sequence.h"
#include "psx/sound_bank.h"
#include "psx/sound_voice.h"
#include <libsnd.h>
#include <string.h>

static hm_sound_control hm_sound;

const hm_sound_control *hm_sound_configuration(void) { return &hm_sound; }

static void hm_sound_require(const char *api) {
    if (!hm_sound.initialized)
        hm_runtime_error(api, "sound library is not initialized");
}

void SsInit(void) {
    hm_sound_sequences_reset();
    hm_sound_voices_reset();
    hm_sound_banks_reset();
    hm_reverb_reset();
    memset(&hm_sound, 0, sizeof(hm_sound));
    hm_sound.initialized = 1;
}

void SsSetTableSize(void *table, s32 sequences, s32 tracks) {
    hm_sound_require("SsSetTableSize");
    if (!table || sequences < 1 || sequences > 16 || tracks != 1)
        hm_runtime_error("SsSetTableSize", "unsupported sequence table configuration");
    hm_sound.table = table;
    hm_sound.sequences = sequences;
    hm_sound.tracks = tracks;
}

void SsSetTickMode(s32 mode) {
    hm_sound_require("SsSetTickMode");
    if (mode != 1)
        hm_runtime_error("SsSetTickMode", "only VBlank tick mode is supported");
    hm_sound.tick_mode = mode;
}

s32 SsSetReservedVoice(s32 voices) {
    hm_sound_require("SsSetReservedVoice");
    if (voices < 0 || voices > 24)
        return -1;
    hm_sound.reserved = voices;
    return voices;
}

void SpuSetCommonAttr(const SpuCommonAttr *attributes) {
    hm_sound_require("SpuSetCommonAttr");
    if (!attributes || (attributes->mask & ~0x2C3u))
        hm_runtime_error("SpuSetCommonAttr", "unsupported common attribute mask");
    u32 mask = attributes->mask;
    if (mask & 1)
        hm_sound.common.mvol.left = attributes->mvol.left;
    if (mask & 2)
        hm_sound.common.mvol.right = attributes->mvol.right;
    if (mask & 0x40)
        hm_sound.common.cd.volume.left = attributes->cd.volume.left;
    if (mask & 0x80)
        hm_sound.common.cd.volume.right = attributes->cd.volume.right;
    if (mask & 0x200)
        hm_sound.common.cd.mix = attributes->cd.mix;
    hm_sound.common.mask |= mask;
}

void SsSetSerialAttr(s32 channel, s32 attribute, s32 enabled) {
    hm_sound_require("SsSetSerialAttr");
    if (channel != 0 || attribute != 0)
        hm_runtime_error("SsSetSerialAttr", "unsupported serial input attribute");
    hm_sound.common.cd.mix = enabled != 0;
}

void SsSetSerialVol(s32 channel, s32 left, s32 right) {
    hm_sound_require("SsSetSerialVol");
    if (channel != 0)
        hm_runtime_error("SsSetSerialVol", "unsupported serial input");
    hm_sound.serial_left = left;
    hm_sound.serial_right = right;
}

void SsSetMVol(s32 left, s32 right) {
    hm_sound_require("SsSetMVol");
    if (left < 0 || left > 127 || right < 0 || right > 127)
        hm_runtime_error("SsSetMVol", "invalid master volume");
    hm_sound.master_left = left;
    hm_sound.master_right = right;
}

void SsSetStereo(void) {
    hm_sound_require("SsSetStereo");
    hm_sound.mono = 0;
}
void SsSetMono(void) {
    hm_sound_require("SsSetMono");
    hm_sound.mono = 1;
}
void SsUtReverbOff(void) {
    hm_sound_require("SsUtReverbOff");
    hm_sound.reverb = 0;
}
void SsUtReverbOn(void) {
    hm_sound_require("SsUtReverbOn");
    hm_sound.reverb = 1;
}
void SsStart(void) {
    hm_sound_require("SsStart");
    if (!hm_sound.table || hm_sound.tick_mode != 1)
        hm_runtime_error("SsStart", "sequence table or VBlank mode is not configured");
    hm_sound.started = 1;
}
void SsEnd(void) {
    hm_sound_require("SsEnd");
    hm_sound.started = 0;
}

void SpuGetAllKeysStatus(u8 status[24]) {
    hm_sound_require("SpuGetAllKeysStatus");
    if (!status)
        hm_runtime_error("SpuGetAllKeysStatus", "null status output");
    for (u32 i = 0; i < 24; ++i)
        status[i] = hm_sound_voice_status(i);
}

s32 SpuGetKeyStatus(u32 voices) {
    hm_sound_require("SpuGetKeyStatus");
    if (voices & 0xFF000000u)
        hm_runtime_error("SpuGetKeyStatus", "invalid voice mask");
    s32 result = 0;
    for (u32 i = 0; i < 24; ++i)
        if (voices & (1u << i))
            result |= hm_sound_voice_status(i);
    return result;
}
