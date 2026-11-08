#ifndef VANILLABTN_SDK_CD_H
#define VANILLABTN_SDK_CD_H

#include <stdbool.h>

/* Mount the imported disc tree, not a MIPS overlay executable. File positions
 * are virtual LBAs; original callers may seek/read them through the SDK API.
 * Mounting and unmounting cancel any outstanding read. */
bool sdk_cd_mount(const char *directory);
void sdk_cd_unmount(void);

#endif
