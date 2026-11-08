#ifndef VANILLABTN_SDK_SOUND_H
#define VANILLABTN_SDK_SOUND_H

#include "ps1_seq.h"
#include "ps1_spu.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct SdkSerialVol {
    int16_t left;
    int16_t right;
} SdkSerialVol;

typedef struct SdkSoundStatus {
    bool initialized;
    bool running;
    bool stereo;
    bool reverb_enabled;
    bool cd_mix;
    int32_t tick_mode;
    int32_t reserved_voices;
    int16_t master_vol_left;
    int16_t master_vol_right;
    int16_t reverb_type;
    int16_t reverb_depth_left;
    int16_t reverb_depth_right;
    int16_t reverb_vol_left;
    int16_t reverb_vol_right;
    int16_t reverb_feedback;
    int16_t reverb_delay;
    int32_t open_vab_count;
    int32_t open_seq_count;
    int32_t playing_seq_count;
} SdkSoundStatus;

void sdk_sound_reset(void);
SdkSerialVol sdk_sound_serial_vol(int s_num);
SdkSoundStatus sdk_sound_status(void);
const Ps1Spu *sdk_sound_spu(void);
void sdk_sound_mix(int16_t *out, int frames);

/* Supply the exact borrowed asset span for the next SsSeqOpen call. The SDK
 * signature omits a length, so unknown/unregistered source buffers are rejected.
 * The view is consumed once; playback borrows the bytes until SsSeqClose. */
void sdk_sound_sequence_source(const void *data, size_t size);

#endif
