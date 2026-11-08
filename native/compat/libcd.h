#ifndef VANILLABTN_COMPAT_LIBCD_H
#define VANILLABTN_COMPAT_LIBCD_H

#include "sdk_types.h"

typedef struct CdlLOC { u_char minute, second, sector, track; } CdlLOC;
typedef struct CdlFILE { CdlLOC pos; u_long size; char name[16]; } CdlFILE;
enum {
    CdlSetloc = 2,
    CdlStandby = 6,
    CdlPause = 9,
    CdlSetfilter = 13,
    CdlSetmode = 14,
    CdlGetlocP = 17,
    CdlReadS = 27,
    CdlComplete = 2,
    CdlDiskError = 5
};

CdlFILE *CdSearchFile(CdlFILE *file, char *name);
CdlLOC *CdIntToPos(int sector, CdlLOC *position);
int CdPosToInt(CdlLOC *position);
int CdControl(u_char command, u_char *parameter, u_char *result);
int CdControlB(u_char command, u_char *parameter, u_char *result);
int CdControlF(u_char command, u_char *parameter);
u_char CdLastCom(void);
int CdSync(int mode, u_char *result);
int CdRead(int sectors, u_long *buffer, int mode);
int CdReadSync(int mode, u_char *result);
void CdReadBreak(void);
void CdFlush(void);

int CdInit(void);

#endif
