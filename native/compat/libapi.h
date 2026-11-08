#ifndef VANILLABTN_COMPAT_LIBAPI_H
#define VANILLABTN_COMPAT_LIBAPI_H

void ChangeClearPAD(long mode);
void SystemError(char type, long errorcode);

long sdk_card_rename(const char *oldpath, const char *newpath);

#ifdef rename
#undef rename
#endif
#define rename sdk_card_rename

#endif
