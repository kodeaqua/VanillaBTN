#include <libgpu.h>
#include <libgs.h>

#include "afile.h"
#include "anim.h"
#include "farm_tables.h"
#include "pad.h"
#include "prim.h"

#include <string.h>

/* Pointer-free records only: host-only state must not be mistaken for a save ABI. */
_Static_assert(sizeof(u_long) == 4, "SDK words cannot grow on LP64 hosts");
_Static_assert(sizeof(u_short) == 2 && sizeof(u_char) == 1, "SDK scalar aliases");
_Static_assert(sizeof(AFileRecord) == 40, "A_FILE directory record");
_Static_assert(__builtin_offsetof(AFileRecord, pos) == 36, "A_FILE position offset");
_Static_assert(sizeof(AnimFile) == 36, "ANI file header");
_Static_assert(__builtin_offsetof(AnimFile, section) == 16, "ANI section table offset");
_Static_assert(sizeof(AnimImage) == 8 && sizeof(AnimPartList) == 4, "ANI image/list layout");
_Static_assert(sizeof(AnimPart) == 12 && sizeof(AnimRange) == 4, "ANI part/range layout");
_Static_assert(sizeof(AnimFrame) == 10, "ANI frame layout");
_Static_assert(__builtin_offsetof(AnimFrame, control) == 7, "ANI frame control offset");
_Static_assert(sizeof(BankImage) == 12 && sizeof(ImageBank) == 4, "Image bank headers");
_Static_assert(sizeof(InputRecord) == 56, "Input replay record");
_Static_assert(sizeof(FarmEstateRecord) == 24, "Estate table row");
_Static_assert(__builtin_offsetof(FarmEstateRecord, position) == 8, "Estate vector offset");
_Static_assert(sizeof(PolyFT4) == sizeof(POLY_FT4), "Game and SDK packet views agree");
_Static_assert(__builtin_offsetof(POLY_FT4, u0) == __builtin_offsetof(PolyFT4, uvClut0),
               "Game word view and UV view agree");
_Static_assert(sizeof(((GsOT *)0)->org) == sizeof(void *), "Native OT descriptor pointers");

int main(void) {
    /* Known GP0 textured-quad bytes plus a DMA length/link word, not a host address. */
    const POLY_FT4 packet = {
        .tag = 0x09FFFFFF, .r0 = 128, .g0 = 64, .b0 = 32, .code = 0x2C,
        .x0 = -1, .y0 = 2, .u0 = 3, .v0 = 4, .clut = 0x1234,
        .x1 = 5, .y1 = 6, .u1 = 7, .v1 = 8, .tpage = 0x0102,
        .x2 = 9, .y2 = 10, .u2 = 11, .v2 = 12,
        .x3 = 13, .y3 = 14, .u3 = 15, .v3 = 16,
    };
    const u8 expected[40] = {
        0xFF, 0xFF, 0xFF, 0x09, 0x80, 0x40, 0x20, 0x2C,
        0xFF, 0xFF, 0x02, 0x00, 0x03, 0x04, 0x34, 0x12,
        0x05, 0x00, 0x06, 0x00, 0x07, 0x08, 0x02, 0x01,
        0x09, 0x00, 0x0A, 0x00, 0x0B, 0x0C, 0x00, 0x00,
        0x0D, 0x00, 0x0E, 0x00, 0x0F, 0x10, 0x00, 0x00,
    };
    return memcmp(&packet, expected, sizeof(expected)) == 0 ? 0 : 1;
}
