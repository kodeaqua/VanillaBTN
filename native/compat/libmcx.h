#ifndef VANILLABTN_COMPAT_LIBMCX_H
#define VANILLABTN_COMPAT_LIBMCX_H

#include <sys/types.h>

#define McxErrSuccess       (0)
#define McxErrNoCard        (1)
#define McxErrInvalid       (2)
#define McxErrNewCard       (3)

#define McxFuncGetApl       (1)
#define McxFuncExecApl      (2)
#define McxFuncGetTime      (3)
#define McxFuncGetMem       (4)
#define McxFuncSetMem       (5)
#define McxFuncShowTrans    (6)
#define McxFuncHideTrans    (7)
#define McxFuncCurrCtrl     (8)
#define McxFuncSetLED       (9)
#define McxFuncGetSerial    (10)
#define McxFuncExecFlag     (11)
#define McxFuncAllInfo      (12)
#define McxFuncFlashAcs     (13)
#define McxFuncReadDev      (14)
#define McxFuncWriteDev     (15)
#define McxFuncCardType     (19)

int McxCardType(long port);
int McxSync(long mode, long *cmd, long *result);
int McxAllInfo(long port, unsigned char *buf);
int McxGetApl(long port, void *result);
int McxGetTime(long port, unsigned char *buf);
int McxHideTrans(long port);
int McxGetSerial(long port, void *serial);
int McxGetMem(long port, void *buf, long addr, long len);
int McxExecFlag(long port, long block, long flag);
int McxExecApl(long port, long apl, long param);
int McxCurrCtrl(long port, long sound, long infrared, long rsv);
int McxFlashAcs(long port, long flag);
int McxSetMem(long port, const void *buf, long addr, long len);
int McxShowTrans(long port, long shown, long timeout);
int McxSetLED(long port, long flag);
int McxReadDev(long port, long dev, const void *fix, void *read);
int McxWriteDev(long port, long dev, const void *fix, const void *read);

#endif /* VANILLABTN_COMPAT_LIBMCX_H */
