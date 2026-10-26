#include <libcd.h>

/* Public CD sector addressing: 75 frames/second, 60 seconds/minute, with the
 * first data LBA at MSF 00:02:00. No SDK implementation is used here. */
static u8 hm_cd_bcd_encode(u32 value) {
    return (u8)(((value / 10) << 4) | (value % 10));
}

static s32 hm_cd_bcd_decode(u8 value) {
    return (value >> 4) * 10 + (value & 15);
}

CdlLOC *CdIntToPos(s32 sector, CdlLOC *position) {
    /* Unsigned addition avoids host UB at the extremes of the input type.
     * Valid BCD positions cover LBA -150 through 449849. */
    u32 frames = (u32)sector + 150;
    position->minute = hm_cd_bcd_encode(frames / (60 * 75));
    position->second = hm_cd_bcd_encode((frames / 75) % 60);
    position->sector = hm_cd_bcd_encode(frames % 75);
    return position;
}

s32 CdPosToInt(const CdlLOC *position) {
    /* cd_audio.c passes &result[5] from an eight-byte response. Never load a
     * fourth byte, including via a four-byte copy or cast. */
    const u8 *msf = (const u8 *)position;
    return (hm_cd_bcd_decode(msf[0]) * 60 + hm_cd_bcd_decode(msf[1])) * 75
           + hm_cd_bcd_decode(msf[2]) - 150;
}
