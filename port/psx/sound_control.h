#ifndef HMBTN_PSX_SOUND_CONTROL_H
#define HMBTN_PSX_SOUND_CONTROL_H

#include <libspu.h>

/* Native configuration consumed by the future mixer/sequencer. This module
 * handles silent initialization; bank/sequence/voice playback still fails
 * explicitly until the corresponding decoder is implemented. */
typedef struct {
    SpuCommonAttr common;
    s32 sequences, tracks, reserved, tick_mode;
    s32 master_left, master_right, serial_left, serial_right;
    int initialized, started, mono, reverb;
    const void *table;
} hm_sound_control;
const hm_sound_control *hm_sound_configuration(void);

#endif
