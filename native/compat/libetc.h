#ifndef VANILLABTN_COMPAT_LIBETC_H
#define VANILLABTN_COMPAT_LIBETC_H

int ResetCallback(void);
int VSync(int mode);

int VSyncCallback(void (*callback)(void));
long SetVideoMode(long mode);

#endif
