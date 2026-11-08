#ifndef VANILLABTN_COMPAT_LIBMCRD_H
#define VANILLABTN_COMPAT_LIBMCRD_H

#include "kernel.h"

#define McFuncExist         (1)
#define McFuncAccept        (2)
#define McFuncReadFile      (3)
#define McFuncWriteFile     (4)
#define McFuncReadData      (5)
#define McFuncWriteData     (6)

#define McErrNone           (0)
#define McErrCardNotExist   (1)
#define McErrCardInvalid    (2)
#define McErrNewCard        (3)
#define McErrNotFormat      (4)
#define McErrFileNotExist   (5)
#define McErrAlreadyExist   (6)
#define McErrBlockFull      (7)
#define McErrExtend         (0x8000)

void MemCardInit(int val);
void MemCardEnd(void);
void MemCardStart(void);
void MemCardStop(void);
int MemCardExist(int chan);
int MemCardAccept(int chan);
int MemCardOpen(int chan, char *file, int flag);
void MemCardClose(void);
int MemCardReadData(unsigned int *adrs, int ofs, int bytes);
int MemCardReadFile(int chan, char *file, unsigned int *adrs, int ofs, int bytes);
int MemCardWriteData(unsigned int *adrs, int ofs, int bytes);
int MemCardWriteFile(int chan, char *file, unsigned int *adrs, int ofs, int bytes);
int MemCardCreateFile(int chan, char *file, int blocks);
int MemCardDeleteFile(int chan, char *file);
int MemCardFormat(int chan);
int MemCardUnformat(int chan);
int MemCardSync(int mode, long *cmds, long *rslt);
int MemCardGetDirentry(int chan, const char *name, struct DIRENTRY *dir, long *files, int ofs, int max);

#endif /* VANILLABTN_COMPAT_LIBMCRD_H */
