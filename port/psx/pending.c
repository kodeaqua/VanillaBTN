/* Named bring-up failures for SDK calls required by compiled game objects.
 * Never return success or replace gameplay with a placeholder. */
#include "compat/fixed_math.h"
#include "psx/unimplemented.h"
#include <libapi.h>
#include <libetc.h>
#include <libgpu.h>
#include <libgs.h>
#include <libgte.h>
#include <libmcrd.h>
#include <libmcx.h>
#include <libpad.h>
#include <libsnd.h>
#include <libspu.h>

void FntFlush(s32 stream) {
    (void)stream;
    hm_unimplemented("FntFlush");
}

void FntLoad(s32 x, s32 y) {
    (void)x;
    (void)y;
    hm_unimplemented("FntLoad");
}

s32 FntOpen(s32 x, s32 y, s32 width, s32 height, s32 background, s32 characters) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)background;
    (void)characters;
    hm_unimplemented("FntOpen");
}

s32 FntPrint(const char *format, ...) {
    (void)format;
    hm_unimplemented("FntPrint");
}

void KanjiFntClose(void) { hm_unimplemented("KanjiFntClose"); }

void KanjiFntFlush(s32 stream) {
    (void)stream;
    hm_unimplemented("KanjiFntFlush");
}

void KanjiFntOpen(s32 x, s32 y, s32 width, s32 height, s32 texture_x, s32 texture_y, s32 clut_x,
                  s32 red, s32 green, s32 blue) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)texture_x;
    (void)texture_y;
    (void)clut_x;
    (void)red;
    (void)green;
    (void)blue;
    hm_unimplemented("KanjiFntOpen");
}

s32 KanjiFntPrint(const char *format, ...) {
    (void)format;
    hm_unimplemented("KanjiFntPrint");
}

s32 McxAllInfo(s32 port, void *output) {
    (void)port;
    (void)output;
    hm_unimplemented("McxAllInfo");
}

s32 McxCurrCtrl(s32 port, s32 sound, s32 infrared, s32 unused) {
    (void)port;
    (void)sound;
    (void)infrared;
    (void)unused;
    hm_unimplemented("McxCurrCtrl");
}

s32 McxExecApl(s32 port, s32 application, u32 offset) {
    (void)port;
    (void)application;
    (void)offset;
    hm_unimplemented("McxExecApl");
}

s32 McxExecFlag(s32 port, s32 block, s32 flag) {
    (void)port;
    (void)block;
    (void)flag;
    hm_unimplemented("McxExecFlag");
}

s32 McxFlashAcs(s32 port, s32 flag) {
    (void)port;
    (void)flag;
    hm_unimplemented("McxFlashAcs");
}

s32 McxGetApl(s32 port, void *output) {
    (void)port;
    (void)output;
    hm_unimplemented("McxGetApl");
}

s32 McxGetMem(s32 port, void *output, u32 offset, u32 bytes) {
    (void)port;
    (void)output;
    (void)offset;
    (void)bytes;
    hm_unimplemented("McxGetMem");
}

s32 McxGetSerial(s32 port, void *output) {
    (void)port;
    (void)output;
    hm_unimplemented("McxGetSerial");
}

s32 McxGetTime(s32 port, void *output) {
    (void)port;
    (void)output;
    hm_unimplemented("McxGetTime");
}

s32 McxHideTrans(s32 port) {
    (void)port;
    hm_unimplemented("McxHideTrans");
}

s32 McxReadDev(s32 port, s32 device, const u8 *fixed, u8 *data) {
    (void)port;
    (void)device;
    (void)fixed;
    (void)data;
    hm_unimplemented("McxReadDev");
}

s32 McxSetLED(s32 port, s32 flag) {
    (void)port;
    (void)flag;
    hm_unimplemented("McxSetLED");
}

s32 McxSetMem(s32 port, const void *input, u32 offset, u32 bytes) {
    (void)port;
    (void)input;
    (void)offset;
    (void)bytes;
    hm_unimplemented("McxSetMem");
}

s32 McxShowTrans(s32 port, s32 shown, s32 timeout) {
    (void)port;
    (void)shown;
    (void)timeout;
    hm_unimplemented("McxShowTrans");
}

s32 McxWriteDev(s32 port, s32 device, const u8 *fixed, const u8 *data) {
    (void)port;
    (void)device;
    (void)fixed;
    (void)data;
    hm_unimplemented("McxWriteDev");
}

void SetDumpFnt(s32 stream) {
    (void)stream;
    hm_unimplemented("SetDumpFnt");
}

s32 hm_psx_csqrt(s32 value) {
    (void)value;
    hm_unimplemented("hm_psx_csqrt");
}

s32 ratan2(s32 y, s32 x) {
    (void)y;
    (void)x;
    hm_unimplemented("ratan2");
}
