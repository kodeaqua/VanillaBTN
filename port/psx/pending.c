/* Named bring-up failures for SDK calls required by compiled game objects.
 * Never return success or replace gameplay with a placeholder. */
#include "compat/fixed_math.h"
#include "psx/unimplemented.h"
#include <libapi.h>
#include <libetc.h>
#include <libgpu.h>
#include <libgs.h>
#include <libgte.h>
#include <libmcrd.h>
#include <libmcx.h>
#include <libpad.h>
#include <libsnd.h>
#include <libspu.h>

void FntFlush(s32 stream) {
    (void)stream;
    hm_unimplemented("FntFlush");
}

void FntLoad(s32 x, s32 y) {
    (void)x;
    (void)y;
    hm_unimplemented("FntLoad");
}

s32 FntOpen(s32 x, s32 y, s32 width, s32 height, s32 background, s32 characters) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)background;
    (void)characters;
    hm_unimplemented("FntOpen");
}

void KanjiFntClose(void) { hm_unimplemented("KanjiFntClose"); }

void KanjiFntFlush(s32 stream) {
    (void)stream;
    hm_unimplemented("KanjiFntFlush");
}

void KanjiFntOpen(s32 x, s32 y, s32 width, s32 height, s32 texture_x, s32 texture_y, s32 clut_x,
                  s32 red, s32 green, s32 blue) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)texture_x;
    (void)texture_y;
    (void)clut_x;
    (void)red;
    (void)green;
    (void)blue;
    hm_unimplemented("KanjiFntOpen");
}

s32 KanjiFntPrint(const char *format, ...) {
    (void)format;
    hm_unimplemented("KanjiFntPrint");
}

void SetDumpFnt(s32 stream) {
    (void)stream;
    hm_unimplemented("SetDumpFnt");
}
