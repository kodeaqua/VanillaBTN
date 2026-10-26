#ifndef HMBTN_PSX_CD_RUNTIME_H
#define HMBTN_PSX_CD_RUNTIME_H

#include "psx/disc.h"

/* Single game-thread calls only. The mounted stream must outlive attachment. */
void hm_cd_attach(hm_disc *disc);
void hm_cd_detach(void);
/* Defined 60 Hz game tick, not an audio or filesystem thread callback. */
void hm_cd_tick(void);
/* Consume one decoded 44.1 kHz CD-input frame on the game thread. Silence when
 * no audio is queued, including before a disc is attached. */
int hm_cd_audio_sample(s16 stereo[2]);

#endif
