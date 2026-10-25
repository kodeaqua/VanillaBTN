#ifndef HMBTN_COMPAT_LIBPAD_H
#define HMBTN_COMPAT_LIBPAD_H

#include "compat/types.h"

/* Native controller-library states; these are not bytes in a pad reply. */
enum {
    PadStateDiscon = 0,
    PadStateFindPad = 1,
    PadStateReqInfo = 2,
    PadStateExecCmd = 5,
    PadStateStable = 6,
    InfoModeCurExID = 2
};
void PadInitDirect(u8 *first, u8 *second);
void PadSetAct(s32 port, u8 *actuators, s32 bytes);
void PadStartCom(void);
s32 PadGetState(s32 port);
s32 PadInfoMode(s32 port, s32 information, s32 index);
s32 PadSetActAlign(s32 port, const u8 *alignment);

#endif
