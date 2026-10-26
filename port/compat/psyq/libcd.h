#ifndef HMBTN_COMPAT_LIBCD_H
#define HMBTN_COMPAT_LIBCD_H

#include "compat/types.h"

/* Setloc/Getloc positions carry three BCD bytes; the fourth byte is reserved.
 * CdPosToInt must also accept the last three bytes of an eight-byte response. */
typedef struct {
    u8 minute, second, sector, track;
} CdlLOC;

/* Source-facing search metadata, not an on-disc record or a binary SDK ABI.
 * Only these two fields are read by the game's CdSearchFile callers. */
typedef struct {
    CdlLOC pos;
    u32 size;
} CdlFILE;

enum {
    CdlSetloc = 0x02,
    CdlStandby = 0x07,
    CdlSetfilter = 0x0D,
    CdlSetmode = 0x0E,
    CdlGetlocP = 0x11,
    CdlReadS = 0x1B
};

_Static_assert(sizeof(CdlLOC) == 4, "CD position buffer size");
_Static_assert(__builtin_offsetof(CdlLOC, sector) == 2, "CD position frame offset");

CdlLOC *CdIntToPos(s32 sector, CdlLOC *position);
s32 CdPosToInt(const CdlLOC *position);
s32 CdInit(void);
CdlFILE *CdSearchFile(CdlFILE *file, const char *name);
s32 CdControl(u8 command, const u8 *parameters, u8 *result);
s32 CdControlB(u8 command, const u8 *parameters, u8 *result);
s32 CdControlF(u8 command, const u8 *parameters);
s32 CdRead(s32 sectors, u32 *destination, s32 mode);
s32 CdReadSync(s32 mode, u8 *result);
s32 CdSync(s32 mode, u8 *result);
s32 CdReadBreak(void);
s32 CdFlush(void);
u8 CdLastCom(void);

#endif
