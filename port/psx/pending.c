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

VECTOR *ApplyMatrixLV(const MATRIX *matrix, const VECTOR *input, VECTOR *output) {
    (void)matrix;
    (void)input;
    (void)output;
    hm_unimplemented("ApplyMatrixLV");
}

s32 ChangeClearPAD(s32 mode) {
    (void)mode;
    hm_unimplemented("ChangeClearPAD");
}

void DrawOTag(u32 *table) {
    (void)table;
    hm_unimplemented("DrawOTag");
}

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

void GsSetAmbient(s32 red, s32 green, s32 blue) {
    (void)red;
    (void)green;
    (void)blue;
    hm_unimplemented("GsSetAmbient");
}

void GsSetFlatLight(s32 index, const GsF_LIGHT *light) {
    (void)index;
    (void)light;
    hm_unimplemented("GsSetFlatLight");
}

void GsSetLightMode(s32 mode) {
    (void)mode;
    hm_unimplemented("GsSetLightMode");
}

void GsSetProjection(s32 distance) {
    (void)distance;
    hm_unimplemented("GsSetProjection");
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

s32 McxCardType(s32 port) {
    (void)port;
    hm_unimplemented("McxCardType");
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

void McxStartCom(void) { hm_unimplemented("McxStartCom"); }

s32 McxSync(s32 mode, s32 *command, s32 *result) {
    (void)mode;
    (void)command;
    (void)result;
    hm_unimplemented("McxSync");
}

s32 McxWriteDev(s32 port, s32 device, const u8 *fixed, const u8 *data) {
    (void)port;
    (void)device;
    (void)fixed;
    (void)data;
    hm_unimplemented("McxWriteDev");
}

s32 MemCardAccept(s32 port) {
    (void)port;
    hm_unimplemented("MemCardAccept");
}

s32 MemCardCreateFile(s32 port, const char *name, s32 blocks) {
    (void)port;
    (void)name;
    (void)blocks;
    hm_unimplemented("MemCardCreateFile");
}

s32 MemCardExist(s32 port) {
    (void)port;
    hm_unimplemented("MemCardExist");
}

s32 MemCardFormat(s32 port) {
    (void)port;
    hm_unimplemented("MemCardFormat");
}

s32 MemCardGetDirentry(s32 port, const char *pattern, struct DIRENTRY *entries, s32 *count,
                       s32 offset, s32 maximum) {
    (void)port;
    (void)pattern;
    (void)entries;
    (void)count;
    (void)offset;
    (void)maximum;
    hm_unimplemented("MemCardGetDirentry");
}

void MemCardInit(s32 mode) {
    (void)mode;
    hm_unimplemented("MemCardInit");
}

s32 MemCardReadFile(s32 port, const char *name, void *buffer, s32 offset, s32 bytes) {
    (void)port;
    (void)name;
    (void)buffer;
    (void)offset;
    (void)bytes;
    hm_unimplemented("MemCardReadFile");
}

void MemCardStart(void) { hm_unimplemented("MemCardStart"); }

s32 MemCardSync(s32 mode, s32 *command, s32 *result) {
    (void)mode;
    (void)command;
    (void)result;
    hm_unimplemented("MemCardSync");
}

s32 MemCardUnformat(s32 port) {
    (void)port;
    hm_unimplemented("MemCardUnformat");
}

s32 MemCardWriteFile(s32 port, const char *name, const void *buffer, s32 offset, s32 bytes) {
    (void)port;
    (void)name;
    (void)buffer;
    (void)offset;
    (void)bytes;
    hm_unimplemented("MemCardWriteFile");
}

s32 MoveImage(const RECT *area, s32 x, s32 y) {
    (void)area;
    (void)x;
    (void)y;
    hm_unimplemented("MoveImage");
}

s32 MoveImage2(const RECT *area, s32 x, s32 y) {
    (void)area;
    (void)x;
    (void)y;
    hm_unimplemented("MoveImage2");
}

void OuterProduct0(VECTOR *first, VECTOR *second, VECTOR *result) {
    (void)first;
    (void)second;
    (void)result;
    hm_unimplemented("OuterProduct0");
}

s32 PadGetState(s32 port) {
    (void)port;
    hm_unimplemented("PadGetState");
}

s32 PadInfoMode(s32 port, s32 information, s32 index) {
    (void)port;
    (void)information;
    (void)index;
    hm_unimplemented("PadInfoMode");
}

void PadInitDirect(u8 *first, u8 *second) {
    (void)first;
    (void)second;
    hm_unimplemented("PadInitDirect");
}

void PadSetAct(s32 port, u8 *actuators, s32 bytes) {
    (void)port;
    (void)actuators;
    (void)bytes;
    hm_unimplemented("PadSetAct");
}

s32 PadSetActAlign(s32 port, const u8 *alignment) {
    (void)port;
    (void)alignment;
    hm_unimplemented("PadSetActAlign");
}

void PadStartCom(void) { hm_unimplemented("PadStartCom"); }

MATRIX *RotMatrix(const SVECTOR *angle, MATRIX *matrix) {
    (void)angle;
    (void)matrix;
    hm_unimplemented("RotMatrix");
}

MATRIX *RotMatrixX(s32 angle, MATRIX *matrix) {
    (void)angle;
    (void)matrix;
    hm_unimplemented("RotMatrixX");
}

MATRIX *RotMatrixY(s32 angle, MATRIX *matrix) {
    (void)angle;
    (void)matrix;
    hm_unimplemented("RotMatrixY");
}

MATRIX *RotMatrixZ(s32 angle, MATRIX *matrix) {
    (void)angle;
    (void)matrix;
    hm_unimplemented("RotMatrixZ");
}

MATRIX *RotMatrixZXY(const SVECTOR *angle, MATRIX *matrix) {
    (void)angle;
    (void)matrix;
    hm_unimplemented("RotMatrixZXY");
}

void SetDumpFnt(s32 stream) {
    (void)stream;
    hm_unimplemented("SetDumpFnt");
}

void SsSeqClose(s32 sequence) {
    (void)sequence;
    hm_unimplemented("SsSeqClose");
}

s32 SsSeqOpen(const u32 *data, s32 bank) {
    (void)data;
    (void)bank;
    hm_unimplemented("SsSeqOpen");
}

void SsSeqPlay(s32 sequence, s32 mode, s32 repeats) {
    (void)sequence;
    (void)mode;
    (void)repeats;
    hm_unimplemented("SsSeqPlay");
}

void SsSeqSetVol(s32 sequence, s32 left, s32 right) {
    (void)sequence;
    (void)left;
    (void)right;
    hm_unimplemented("SsSeqSetVol");
}

void SsSeqStop(s32 sequence) {
    (void)sequence;
    hm_unimplemented("SsSeqStop");
}

s32 SsUtKeyOffV(s32 voice) {
    (void)voice;
    hm_unimplemented("SsUtKeyOffV");
}

s32 SsUtKeyOnV(s32 voice, s32 bank, s32 program, s32 tone, s32 note, s32 fine, s32 left,
               s32 right) {
    (void)voice;
    (void)bank;
    (void)program;
    (void)tone;
    (void)note;
    (void)fine;
    (void)left;
    (void)right;
    hm_unimplemented("SsUtKeyOnV");
}

void SsUtReverbOn(void) { hm_unimplemented("SsUtReverbOn"); }

void SsUtSetReverbDepth(s32 left, s32 right) {
    (void)left;
    (void)right;
    hm_unimplemented("SsUtSetReverbDepth");
}

s32 SsUtSetReverbType(s32 type) {
    (void)type;
    hm_unimplemented("SsUtSetReverbType");
}

void SsUtSetVVol(s32 voice, s32 left, s32 right) {
    (void)voice;
    (void)left;
    (void)right;
    hm_unimplemented("SsUtSetVVol");
}

void SsVabClose(s32 bank) {
    (void)bank;
    hm_unimplemented("SsVabClose");
}

s32 SsVabOpenHead(const void *header, s32 bank) {
    (void)header;
    (void)bank;
    hm_unimplemented("SsVabOpenHead");
}

s32 SsVabTransBodyPartly(const void *data, s32 bytes, s32 bank) {
    (void)data;
    (void)bytes;
    (void)bank;
    hm_unimplemented("SsVabTransBodyPartly");
}

s32 SsVabTransCompleted(s32 mode) {
    (void)mode;
    hm_unimplemented("SsVabTransCompleted");
}

void VectorNormalS(const VECTOR *input, SVECTOR *output) {
    (void)input;
    (void)output;
    hm_unimplemented("VectorNormalS");
}

void VectorNormalSS(const SVECTOR *input, SVECTOR *output) {
    (void)input;
    (void)output;
    hm_unimplemented("VectorNormalSS");
}

s32 hm_psx_ccos(s32 angle) {
    (void)angle;
    hm_unimplemented("hm_psx_ccos");
}

s32 hm_psx_csin(s32 angle) {
    (void)angle;
    hm_unimplemented("hm_psx_csin");
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
