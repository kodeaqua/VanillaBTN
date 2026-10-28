#include "compat/family_names.h"
#include "psx/error.h"
#include <string.h>
extern u32 family_tables[5];
static u16 names[5][9];
static int ready;
static u32 word(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
hm_disc_result hm_family_names_load(hm_disc *disc) {
    ready = 0;
    memset(names, 0xff, sizeof names);
    if (!disc || !disc->mounted)
        return HM_DISC_FORMAT;
    hm_disc_entry header, archive;
    hm_disc_result result = hm_disc_find(disc, "A_FILE.HDT;1", &header);
    if (result != HM_DISC_OK)
        return result;
    result = hm_disc_find(disc, "A_FILE.BIN;1", &archive);
    if (result != HM_DISC_OK)
        return result;
    u8 range[8];
    result = hm_disc_read(disc, &header, 11 * 4, range, sizeof range);
    if (result != HM_DISC_OK)
        return result;
    u32 start = word(range), end = word(range + 4);
    if (start > end || end > archive.size)
        return HM_DISC_RANGE;
    for (u32 i = 0; i < 5; ++i) {
        /* The original low overlay's link base; pointers are metadata, never
         * host addresses. Do not ship a copy of the glyph strings. */
        if (family_tables[i] < 0x800b7e20u)
            return HM_DISC_RANGE;
        u32 offset = family_tables[i] - 0x800b7e20u;
        u8 glyphs[18];
        if (offset > end - start || sizeof glyphs > end - start - offset)
            return HM_DISC_RANGE;
        result = hm_disc_read(disc, &archive, start + offset, glyphs, sizeof glyphs);
        if (result != HM_DISC_OK)
            return result;
        int terminated = 0;
        for (u32 j = 0; j < 9; ++j) {
            u16 glyph = (u16)((u16)glyphs[j * 2] | ((u16)glyphs[j * 2 + 1] << 8));
            names[i][j] = glyph;
            if (glyph == 0xffff) {
                terminated = 1;
                break;
            }
        }
        if (!terminated)
            return HM_DISC_FORMAT;
    }
    ready = 1;
    return HM_DISC_OK;
}
static u16 fallback_empty_name[2] = {0xffff, 0xffff};
u16 *hm_family_name(s32 index) {
    if (!ready || index < 0 || index >= 5)
        return fallback_empty_name;
    return names[index];
}
