#include "sdk_sound.h"
#include <libsnd.h>
#include <stdlib.h>

static SdkSerialVol serial_vol[2];

void sdk_sound_reset(void) {
    serial_vol[0] = (SdkSerialVol){0, 0};
    serial_vol[1] = (SdkSerialVol){0, 0};
}

void SsSetSerialVol(short s_num, short voll, short volr) {
    if (s_num < 0 || s_num > 1 || voll < 0 || voll > 127 || volr < 0 || volr > 127) {
        abort();
    }
    serial_vol[s_num] = (SdkSerialVol){voll, volr};
}

SdkSerialVol sdk_sound_serial_vol(int s_num) {
    if (s_num < 0 || s_num > 1) return (SdkSerialVol){0, 0};
    return serial_vol[s_num];
}
