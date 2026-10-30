#ifndef HMBTN_COMPAT_LIBMCX_H
#define HMBTN_COMPAT_LIBMCX_H

#include "compat/types.h"

enum { McxErrSuccess = 0, McxErrNoCard = 1, McxErrInvalid = 2, McxErrNewCard = 3 };
/* Opaque native completion ID, not a PocketStation wire command or SDK ABI. */
enum { McxFuncExecFlag = 0x100 };

void McxStartCom(void);
s32 McxSync(s32 mode, long *command, long *result);
s32 McxCardType(s32 port);
s32 McxAllInfo(s32 port, void *output);
s32 McxGetApl(s32 port, void *output);
s32 McxGetTime(s32 port, void *output);
s32 McxHideTrans(s32 port);
s32 McxGetSerial(s32 port, void *output);
s32 McxGetMem(s32 port, void *output, u32 offset, u32 bytes);
s32 McxSetMem(s32 port, const void *input, u32 offset, u32 bytes);
s32 McxExecFlag(s32 port, s32 block, s32 flag);
s32 McxExecApl(s32 port, s32 application, u32 offset);
s32 McxCurrCtrl(s32 port, s32 sound, s32 infrared, s32 unused);
s32 McxFlashAcs(s32 port, s32 flag);
s32 McxShowTrans(s32 port, s32 shown, s32 timeout);
s32 McxSetLED(s32 port, s32 flag);
s32 McxReadDev(s32 port, s32 device, const u8 *fixed, u8 *data);
s32 McxWriteDev(s32 port, s32 device, const u8 *fixed, const u8 *data);

#endif
