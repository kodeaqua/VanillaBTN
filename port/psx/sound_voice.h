#ifndef HMBTN_PSX_SOUND_VOICE_H
#define HMBTN_PSX_SOUND_VOICE_H
#include "compat/types.h"
void hm_sound_voices_reset(void);
u8 hm_sound_voice_status(u32 voice);
int hm_sound_bank_has_voices(s32 bank);
void hm_sound_mix(s16 *stereo, u32 frames);
u32 hm_sound_note_pitch(s32 note, s32 fine, s32 center, s32 shift);
void hm_sound_voice_input_volume(u32 voice, s32 left, s32 right);
void hm_sound_voice_bend(u32 voice, s32 fine_offset);
void hm_sound_tick(void);
void hm_sound_set_sink(void (*sink)(void *context, const s16 *stereo, u32 frames), void *context);
#endif
