#ifndef VANILLABTN_SDK_CD_H
#define VANILLABTN_SDK_CD_H

#include <stdbool.h>
#include <stdint.h>

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

/* Configure seamless 0ms CD-XA looping */
void sdk_cd_set_seamless_loop(bool enabled);
bool sdk_cd_get_seamless_loop(void);

/* Decode the mounted drive stream to unscaled stereo PCM. This advances the
 * drive even while the SPU CD input is muted; no user files are written. */
void sdk_cd_mix(int16_t *out, int frames);

#endif
