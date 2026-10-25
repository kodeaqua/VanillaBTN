#include <libcd.h>

#include "psx/unimplemented.h"

/* Disc access is unavailable during bring-up. These must never pretend that a
 * read completed or that file metadata exists without validating user data. */
s32 CdInit(void) {
    hm_unimplemented("CdInit");
}

CdlFILE *CdSearchFile(CdlFILE *file, const char *name) {
    (void)file;
    (void)name;
    hm_unimplemented("CdSearchFile");
}

s32 CdControl(u8 command, const u8 *parameters, u8 *result) {
    (void)command;
    (void)parameters;
    (void)result;
    hm_unimplemented("CdControl");
}

s32 CdRead(s32 sectors, u32 *destination, s32 mode) {
    (void)sectors;
    (void)destination;
    (void)mode;
    hm_unimplemented("CdRead");
}

s32 CdReadSync(s32 mode, u8 *result) {
    (void)mode;
    (void)result;
    hm_unimplemented("CdReadSync");
}

s32 CdSync(s32 mode, u8 *result) {
    (void)mode;
    (void)result;
    hm_unimplemented("CdSync");
}

s32 CdReadBreak(void) {
    hm_unimplemented("CdReadBreak");
}

s32 CdFlush(void) {
    hm_unimplemented("CdFlush");
}
