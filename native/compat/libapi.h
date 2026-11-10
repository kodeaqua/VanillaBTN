#ifndef VANILLABTN_COMPAT_LIBAPI_H
#define VANILLABTN_COMPAT_LIBAPI_H

#include <stdio.h>

void ChangeClearPAD(long mode);
void SystemError(char type, long errorcode);

int sdk_card_rename(const char *oldpath, const char *newpath);

#ifdef rename
#undef rename
#endif
#define rename sdk_card_rename

void SetMem(long megabytes);

#endif
