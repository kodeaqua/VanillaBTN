#ifndef HMBTN_PSX_CD_RUNTIME_H
#define HMBTN_PSX_CD_RUNTIME_H

#include "psx/disc.h"

/* Single game-thread calls only. The mounted stream must outlive attachment. */
void hm_cd_attach(hm_disc *disc);
void hm_cd_detach(void);
/* Defined 60 Hz game tick, not an audio or filesystem thread callback. */
void hm_cd_tick(void);

#endif
