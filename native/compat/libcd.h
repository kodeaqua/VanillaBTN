#ifndef VANILLABTN_COMPAT_LIBCD_H
#define VANILLABTN_COMPAT_LIBCD_H

#include "sdk_types.h"

typedef struct CdlLOC { u_char minute, second, sector, track; } CdlLOC;
typedef struct CdlFILE { CdlLOC pos; u_long size; char name[16]; } CdlFILE;
enum { CdlSetloc = 2, CdlSetmode = 14, CdlComplete = 2 };

CdlFILE *CdSearchFile(CdlFILE *file, char *name);
CdlLOC *CdIntToPos(int sector, CdlLOC *position);
int CdPosToInt(CdlLOC *position);
int CdControl(u_char command, u_char *parameter, u_char *result);
int CdSync(int mode, u_char *result);
int CdRead(int sectors, u_long *buffer, int mode);
int CdReadSync(int mode, u_char *result);
void CdReadBreak(void);
void CdFlush(void);

#endif
