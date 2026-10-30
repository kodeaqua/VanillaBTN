#ifndef HMBTN_COMPAT_LIBMCRD_H
#define HMBTN_COMPAT_LIBMCRD_H

#include "compat/types.h"
#include <kernel.h>

enum {
    McErrNone = 0,
    McErrCardNotExist = 1,
    McErrCardInvalid = 2,
    McErrNewCard = 3,
    McErrNotFormat = 4,
    McErrFileNotExist = 5,
    McErrAlreadyExist = 6,
    McErrBlockFull = 7
};

void MemCardInit(s32 mode);
void MemCardStart(void);
s32 MemCardSync(s32 mode, long *command, long *result);
s32 MemCardExist(s32 port);
s32 MemCardAccept(s32 port);
s32 MemCardFormat(s32 port);
s32 MemCardUnformat(s32 port);
s32 MemCardGetDirentry(s32 port, const char *pattern, struct DIRENTRY *entries, long *count,
                       s32 offset, s32 maximum);
s32 MemCardReadFile(s32 port, const char *name, void *buffer, s32 offset, s32 bytes);
s32 MemCardWriteFile(s32 port, const char *name, const void *buffer, s32 offset, s32 bytes);
s32 MemCardCreateFile(s32 port, const char *name, s32 blocks);

#endif
