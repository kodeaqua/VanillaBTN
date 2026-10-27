#include "afile.h"
#include <libetc.h>
/* CMake remaps the minigame overlays and farm status pages: their asset polls run in
 * blocking startup loops. Ordinary per-frame asynchronous reads keep their
 * original timing. A completed read and request initiation never add a tick. */
u8 *hm_blocking_asset_read(s32 file, s32 offset, s32 bytes, u8 *destination, s32 align,
                           s32 restart) {
    u8 *result = afile_read_bytes(file, offset, bytes, destination, align, restart);
    if (!restart && !result)
        VSync(0);
    return result;
}
