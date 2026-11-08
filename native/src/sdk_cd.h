#ifndef VANILLABTN_SDK_CD_H
#define VANILLABTN_SDK_CD_H

#include <stdbool.h>

/* Mount the imported disc tree, not a MIPS overlay executable. File positions
 * are virtual LBAs; original callers may seek/read them through the SDK API.
 * Mounting and unmounting cancel any outstanding read. */
bool sdk_cd_mount(const char *directory);
void sdk_cd_unmount(void);

typedef struct SdkCdXaStatus {
    bool streaming;
    unsigned char mode;
    unsigned char file;
    unsigned char channel;
    unsigned long sector;
} SdkCdXaStatus;

SdkCdXaStatus sdk_cd_xa_status(void);

#endif
