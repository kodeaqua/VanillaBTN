#ifndef VANILLABTN_COMPAT_LIBMCX_H
#define VANILLABTN_COMPAT_LIBMCX_H

#include <sys/types.h>

#define McxErrSuccess       (0)
#define McxErrInvalid       (1)
#define McxErrNewCard       (2)
#define McxErrNoCard        (3)

#define McxFuncExecFlag     (0x10)

int McxCardType(long port);
int McxSync(long port, long *cmd, long *result);

#endif /* VANILLABTN_COMPAT_LIBMCX_H */
