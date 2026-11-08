#ifndef VANILLABTN_SDK_SOUND_H
#define VANILLABTN_SDK_SOUND_H

#include <stdint.h>

typedef struct SdkSerialVol {
    int16_t left;
    int16_t right;
} SdkSerialVol;

void sdk_sound_reset(void);
SdkSerialVol sdk_sound_serial_vol(int s_num);

#endif
