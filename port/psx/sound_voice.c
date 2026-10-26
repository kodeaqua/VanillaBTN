#include "psx/sound_voice.h"
#include "psx/cd_runtime.h"
#include "psx/error.h"
#include "psx/sequence.h"
#include "psx/sound_bank.h"
#include "psx/sound_control.h"
#include "psx/spu_voice.h"
#include <libsnd.h>
#include <string.h>

static hm_spu_voice hm_voices[24];
static void (*hm_audio_sink)(void *, const s16 *, u32);
static void *hm_audio_context;
static s16 hm_tick_audio[735 * 2];
static struct {
    s32 bank, program, tone, note, fine, gain_left, gain_right;
    hm_sound_samples *samples;
} hm_voice_ids[24];
static const u16 hm_pitch[1524] = {
#include "psx/spu_pitch.inc"
};

void hm_sound_voices_reset(void) {
    for (u32 i = 0; i < 24; ++i)
        hm_sound_samples_release(hm_voice_ids[i].samples);
    memset(hm_voices, 0, sizeof(hm_voices));
    memset(hm_voice_ids, 0, sizeof(hm_voice_ids));
}
u8 hm_sound_voice_status(u32 voice) {
    if (voice >= 24)
        hm_runtime_error("SPU voice status", "invalid voice");
    const hm_adsr *envelope = &hm_voices[voice].envelope;
    if (envelope->phase == HM_ADSR_OFF)
        return 0;
    if (envelope->phase == HM_ADSR_RELEASE)
        return envelope->level ? 2 : 0;
    return envelope->level ? 1 : 3;
}
int hm_sound_bank_has_voices(s32 bank) {
    for (u32 i = 0; i < 24; ++i)
        if (hm_voice_ids[i].bank == bank && hm_voices[i].envelope.phase != HM_ADSR_OFF)
            return 1;
    return 0;
}
u32 hm_sound_note_pitch(s32 note, s32 fine, s32 center, s32 shift) {
    if (note < 0 || note > 127 || fine < 0 || fine > 127 || center < 0 || center > 127 ||
        shift < 0 || shift > 127)
        hm_runtime_error("hm_sound_note_pitch", "invalid note or fine tuning");
    s32 offset = (note - center) * 127 + fine - shift;
    s32 octave = offset >= 0 ? offset / 1524 : -1 - ((-1 - offset) / 1524);
    u32 fraction = (u32)(offset - octave * 1524);
    u32 pitch = hm_pitch[fraction];
    if (octave >= 2)
        return 0x4000;
    if (octave >= 0)
        pitch <<= (u32)octave;
    else if (octave > -32)
        pitch >>= (u32)-octave;
    else
        pitch = 0;
    return pitch > 0x4000 ? 0x4000 : pitch;
}

static hm_spu_voice *hm_sound_voice(s32 voice, const char *api) {
    if (voice < 0 || voice >= 24)
        hm_runtime_error(api, "invalid voice");
    return hm_voices + voice;
}
s32 SsUtKeyOffV(s32 voice) {
    hm_spu_voice *state = hm_sound_voice(voice, "SsUtKeyOffV");
    hm_spu_adsr_key_off(&state->envelope);
    return 0;
}
void SsUtAllKeyOff(s32 mode) {
    if (mode)
        hm_runtime_error("SsUtAllKeyOff", "unsupported key-off mode");
    for (s32 i = 0; i < 24; ++i)
        SsUtKeyOffV(i);
}
s32 SsUtKeyOnV(s32 voice, s32 bank, s32 program, s32 tone, s32 note, s32 fine, s32 left,
               s32 right) {
    hm_spu_voice *state = hm_sound_voice(voice, "SsUtKeyOnV");
    const hm_sound_bank *vab = hm_sound_bank_get(bank);
    const u8 *definition = hm_sound_bank_tone(bank, (u32)program, (u32)tone);
    if (!vab || !definition || note < 0 || note > 127 || fine < 0 || fine > 127 || left < 0 ||
        left > 127 || right < 0 || right > 127)
        hm_runtime_error("SsUtKeyOnV", "invalid bank, tone, note or volume");
    u32 bytes;
    u32 sample = (u32)definition[22] | ((u32)definition[23] << 8);
    const u8 *data = hm_sound_bank_sample(bank, sample, &bytes);
    if (!data)
        hm_runtime_error("SsUtKeyOnV", "invalid or incomplete sample asset");
    hm_sound_samples_release(hm_voice_ids[voice].samples);
    hm_voice_ids[voice].samples = hm_sound_samples_retain(bank);
    const u8 *prog = vab->header + 32 + (u32)program * 16;
    s32 gain = 32767 * definition[2] / 127 * prog[1] / 127 * vab->header[24] / 127;
    s32 pan = definition[3] + prog[4] + vab->header[25] - 128;
    if (pan < 0)
        pan = 0;
    if (pan > 127)
        pan = 127;
    s32 gain_left = gain * (pan < 64 ? 64 : 127 - pan) / 64;
    s32 gain_right = gain * (pan > 64 ? 64 : pan) / 64;
    if (pan == 64)
        gain_right = gain_left = gain;
    u16 adsr1 = (u16)((u16)definition[16] | ((u16)definition[17] << 8));
    u16 adsr2 = (u16)((u16)definition[18] | ((u16)definition[19] << 8));
    hm_spu_voice_start(state, data, bytes,
                       hm_sound_note_pitch(note, fine, definition[4], definition[5]), adsr1, adsr2,
                       (s16)(gain_left * left / 127), (s16)(gain_right * right / 127));
    hm_voice_ids[voice].bank = bank;
    hm_voice_ids[voice].program = program;
    hm_voice_ids[voice].tone = tone;
    hm_voice_ids[voice].note = note;
    state->reverb = (definition[1] & 4) != 0;
    hm_voice_ids[voice].fine = fine;
    hm_voice_ids[voice].gain_left = gain_left;
    hm_voice_ids[voice].gain_right = gain_right;
    hm_sound_sequence_forget_voice((u32)voice);
    return voice;
}
void hm_sound_voice_input_volume(u32 voice, s32 left, s32 right) {
    hm_spu_voice *state = hm_sound_voice((s32)voice, "SEQ voice volume");
    if (left < 0 || left > 127 || right < 0 || right > 127)
        hm_runtime_error("SEQ voice volume", "invalid volume");
    state->left = (s16)(hm_voice_ids[voice].gain_left * left / 127);
    state->right = (s16)(hm_voice_ids[voice].gain_right * right / 127);
}
void hm_sound_voice_bend(u32 voice, s32 fine_offset) {
    hm_spu_voice *state = hm_sound_voice((s32)voice, "SEQ pitch bend");
    const u8 *tone = hm_sound_bank_tone(hm_voice_ids[voice].bank, (u32)hm_voice_ids[voice].program,
                                        (u32)hm_voice_ids[voice].tone);
    if (!tone)
        hm_runtime_error("SEQ pitch bend", "missing bank tone");
    s32 note = hm_voice_ids[voice].note * 127 + hm_voice_ids[voice].fine + fine_offset;
    if (note < 0)
        note = 0;
    if (note > 127 * 127)
        note = 127 * 127;
    state->pitch = hm_sound_note_pitch(note / 127, note % 127, tone[4], tone[5]);
}
s32 SsUtKeyOn(s32 bank, s32 program, s32 tone, s32 note, s32 fine, s32 left, s32 right) {
    s32 count = hm_sound_configuration()->reserved;
    for (s32 voice = 0; voice < count; ++voice)
        if (!hm_sound_voice_status((u32)voice))
            return SsUtKeyOnV(voice, bank, program, tone, note, fine, left, right);
    return -1;
}
void SsUtKeyOff(s32 voice, s32 bank, s32 program, s32 tone, s32 note) {
    (void)hm_sound_voice(voice, "SsUtKeyOff");
    if (hm_voice_ids[voice].bank == bank && hm_voice_ids[voice].program == program &&
        hm_voice_ids[voice].tone == tone && hm_voice_ids[voice].note == note)
        SsUtKeyOffV(voice);
}
void SsUtSetVVol(s32 voice, s32 left, s32 right) {
    hm_spu_voice *state = hm_sound_voice(voice, "SsUtSetVVol");
    if (left < 0 || left > 127 || right < 0 || right > 127)
        hm_runtime_error("SsUtSetVVol", "invalid volume");
    state->left = (s16)(left * 32767 / 127);
    state->right = (s16)(right * 32767 / 127);
}
void SsVoKeyOff(s32 voice_bank, s32 mode) {
    if (mode)
        hm_runtime_error("SsVoKeyOff", "unsupported key-off mode");
    s32 voice = voice_bank & 255;
    (void)hm_sound_voice(voice, "SsVoKeyOff");
    if (hm_voice_ids[voice].bank == (voice_bank >> 8))
        SsUtKeyOffV(voice);
}
void hm_sound_mix(s16 *stereo, u32 frames) {
    if (!stereo)
        hm_runtime_error("hm_sound_mix", "null audio output");
    hm_spu_voice_mix_reverb(hm_voices, stereo, frames);
    for (u32 i = 0; i < 24; ++i)
        if (hm_voices[i].envelope.phase == HM_ADSR_OFF) {
            hm_sound_samples_release(hm_voice_ids[i].samples);
            hm_voice_ids[i].samples = NULL;
        }
    const hm_sound_control *control = hm_sound_configuration();
    for (u32 i = 0; i < frames; ++i) {
        s32 left = stereo[i * 2], right = stereo[i * 2 + 1];
        s16 cd[2];
        hm_cd_audio_sample(cd);
        if (control->common.cd.mix) {
            left += cd[0] * control->serial_left / 127;
            right += cd[1] * control->serial_right / 127;
        }
        if (control->mono)
            left = right = (left + right) / 2;
        left = left * control->master_left / 127;
        right = right * control->master_right / 127;
        stereo[i * 2] = (s16)(left < -32768 ? -32768 : left > 32767 ? 32767 : left);
        stereo[i * 2 + 1] = (s16)(right < -32768 ? -32768 : right > 32767 ? 32767 : right);
    }
}
void hm_sound_set_sink(void (*sink)(void *, const s16 *, u32), void *context) {
    hm_audio_sink = sink;
    hm_audio_context = context;
}
void hm_sound_tick(void) {
    if (!hm_sound_configuration()->initialized)
        return;
    hm_sound_sequence_tick();
    hm_sound_mix(hm_tick_audio, 735);
    if (hm_audio_sink)
        hm_audio_sink(hm_audio_context, hm_tick_audio, 735);
}
