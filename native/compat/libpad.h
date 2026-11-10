#ifndef VANILLABTN_COMPAT_LIBPAD_H
#define VANILLABTN_COMPAT_LIBPAD_H

enum {
    PadStateDiscon = 0, PadStateFindPad = 1, PadStateReqInfo = 4,
    PadStateExecCmd = 5, PadStateStable = 6, InfoModeCurExID = 2
};
void PadInitDirect(unsigned char *port1, unsigned char *port2);
void PadStartCom(void);
void PadStopCom(void);
int PadGetState(int port);
int PadInfoMode(int port, int query, int index);
int PadSetActAlign(int port, unsigned char *alignment);
void PadSetAct(int port, unsigned char *data, int length);

#endif
