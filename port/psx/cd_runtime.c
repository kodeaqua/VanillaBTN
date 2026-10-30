#include "psx/cd_runtime.h"
#include "psx/error.h"
#include "psx/unimplemented.h"
#include "psx/xa_decode.h"
#include <libcd.h>
#include <string.h>

static struct {
    hm_disc *disc;
    u8 *destination;
    u32 position, remaining, fraction;
    u8 mode, xa_mode, last_command, response[8];
    int xa_mode_valid;
    int failed;
    int command_failed;
    int streaming;
    u8 filter_file, filter_channel;
    hm_xa_decoder decoder;
    s16 audio[8192][2];
    u32 audio_read, audio_count;
} hm_cd;

static void hm_cd_stop_xa(void) {
    hm_cd.streaming = 0;
    hm_cd.audio_read = hm_cd.audio_count = 0;
    hm_xa_decoder_reset(&hm_cd.decoder);
}

static s32 hm_cd_reject_control(u8 command, u8 *result) {
    hm_cd.response[0] = 3;
    hm_cd.command_failed = 1;
    hm_cd.last_command = command;
    if (result)
        memcpy(result, hm_cd.response, sizeof(hm_cd.response));
    return 0;
}

int hm_cd_audio_sample(s16 stereo[2]) {
    stereo[0] = stereo[1] = 0;
    if (!hm_cd.audio_count)
        return 0;
    stereo[0] = hm_cd.audio[hm_cd.audio_read][0];
    stereo[1] = hm_cd.audio[hm_cd.audio_read][1];
    hm_cd.audio_read = (hm_cd.audio_read + 1) & 8191;
    --hm_cd.audio_count;
    return 1;
}

static void hm_cd_require(const char *api) {
    if (!hm_cd.disc || !hm_cd.disc->mounted)
        hm_runtime_error(api, "no validated disc is attached");
}

void hm_cd_detach(void) { memset(&hm_cd, 0, sizeof(hm_cd)); }

void hm_cd_attach(hm_disc *disc) {
    hm_cd_detach();
    if (!disc || !disc->mounted)
        hm_runtime_error("hm_cd_attach", "disc is not validated");
    hm_cd.disc = disc;
    hm_cd.response[0] = 2; /* Motor on, no pending controller error. */
}

s32 CdInit(void) {
    hm_cd_require("CdInit");
    return 1;
}

CdlFILE *CdSearchFile(CdlFILE *file, const char *name) {
    hm_cd_require("CdSearchFile");
    if (!file || !name)
        hm_runtime_error("CdSearchFile", "null search argument");
    hm_disc_entry entry;
    hm_disc_result result = hm_disc_find(hm_cd.disc, name, &entry);
    if (result == HM_DISC_NOT_FOUND)
        return NULL;
    if (result != HM_DISC_OK)
        hm_runtime_error("CdSearchFile", hm_disc_error(result));
    if (entry.flags & 2)
        return NULL;
    CdIntToPos((s32)entry.sector, &file->pos);
    file->pos.track = 0;
    file->size = entry.size;
    return file;
}

static int hm_cd_bcd(u8 value, u32 limit) {
    return (value & 15) < 10 && (value >> 4) < 10 && (u32)(value >> 4) * 10 + (value & 15) < limit;
}

s32 CdControl(u8 command, const u8 *parameters, u8 *result) {
    hm_cd_require("CdControl");
    memset(hm_cd.response, 0, sizeof(hm_cd.response));
    hm_cd.response[0] = 2;
    hm_cd.command_failed = 0;
    switch (command) {
    case CdlSetloc: {
        if (!parameters || !hm_cd_bcd(parameters[0], 100) || !hm_cd_bcd(parameters[1], 60) ||
            !hm_cd_bcd(parameters[2], 75))
            hm_runtime_error("CdControl", "invalid Setloc position");
        s32 lba = CdPosToInt((const CdlLOC *)parameters);
        if (lba < 0 || (u32)lba >= hm_cd.disc->sectors)
            hm_runtime_error("CdControl", "Setloc outside the track");
        if (hm_cd.remaining)
            hm_runtime_error("CdControl", "Setloc during a data read");
        hm_cd_stop_xa();
        hm_cd.position = (u32)lba;
        break;
    }
    case CdlSetmode:
        if (!parameters)
            hm_runtime_error("CdControl", "missing mode parameter");
        /* 2340-byte transfers and CDDA need a different destination contract. */
        if (parameters[0] & 0x21)
            hm_runtime_error("CdControl", "unsupported sector transfer mode");
        hm_cd.mode = parameters[0];
        if (hm_cd.mode & 0x40) {
            hm_cd.xa_mode = hm_cd.mode;
            hm_cd.xa_mode_valid = 1;
        }
        break;
    case CdlStandby:
    case 9: /* Pause */
        hm_cd_stop_xa();
        if (command == CdlStandby)
            hm_cd.xa_mode_valid = 0;
        hm_cd.remaining = 0;
        hm_cd.destination = NULL;
        break;
    case CdlSetfilter:
        if (!parameters)
            hm_runtime_error("CdControl", "missing filter parameters");
        hm_cd.filter_file = parameters[0];
        hm_cd.filter_channel = parameters[1];
        break;
    case CdlReadS: {
        if (hm_cd.remaining)
            return hm_cd_reject_control(command, result);
        /* CdRead(mode) can leave the drive in data-only mode while the game still
         * has an XA track configured. Restore that track's last XA mode when its
         * fade/restart path issues CdlReadS. */
        if (!(hm_cd.mode & 0x40)) {
            if (!hm_cd.xa_mode_valid)
                return hm_cd_reject_control(command, result);
            hm_cd.mode = hm_cd.xa_mode;
        }
        if (parameters) {
            s32 position = CdPosToInt((const CdlLOC *)parameters);
            if (position < 0 || (u32)position >= hm_cd.disc->sectors)
                hm_runtime_error("CdControl", "XA start outside the track");
            hm_cd.position = (u32)position;
        }
        hm_cd_stop_xa();
        hm_cd.streaming = 1;
        hm_cd.fraction = 0;
        hm_cd.failed = 0;
        break;
    }
    case CdlGetlocP: {
        CdlLOC absolute;
        CdIntToPos((s32)hm_cd.position, &absolute);
        hm_cd.response[0] = hm_cd.response[1] = 1;
        u32 seconds = hm_cd.position / 75;
        u32 minute = seconds / 60, second = seconds % 60, sector = hm_cd.position % 75;
        hm_cd.response[2] = (u8)((minute / 10 << 4) | minute % 10);
        hm_cd.response[3] = (u8)((second / 10 << 4) | second % 10);
        hm_cd.response[4] = (u8)((sector / 10 << 4) | sector % 10);
        memcpy(hm_cd.response + 5, &absolute, 3);
        break;
    }
    default:
        hm_cd.response[0] = 0;
        break;
    }
    hm_cd.last_command = command;
    if (result)
        memcpy(result, hm_cd.response, sizeof(hm_cd.response));
    return 1;
}

s32 CdControlB(u8 command, const u8 *parameters, u8 *result) {
    return CdControl(command, parameters, result);
}

s32 CdControlF(u8 command, const u8 *parameters) { return CdControl(command, parameters, NULL); }

s32 CdRead(s32 sectors, u32 *destination, s32 mode) {
    hm_cd_require("CdRead");
    if (sectors <= 0 || !destination || hm_cd.remaining || mode < 0 || (mode & ~0x80))
        return 0;
    if ((u32)sectors > hm_cd.disc->sectors - hm_cd.position)
        return 0;
    hm_cd_stop_xa();
    hm_cd.destination = (u8 *)destination;
    hm_cd.remaining = (u32)sectors;
    hm_cd.mode = (u8)mode;
    hm_cd.command_failed = 0;
    hm_cd.fraction = 0;
    hm_cd.failed = 0;
    hm_cd.response[0] = 2;
    return 1;
}

static void hm_cd_transfer(u32 budget) {
    while (hm_cd.remaining && budget--) {
        hm_disc_result result = hm_disc_sector(hm_cd.disc, hm_cd.position, hm_cd.destination);
        if (result != HM_DISC_OK) {
            hm_cd.failed = 1;
            hm_cd.remaining = 0;
            hm_cd.destination = NULL;
            hm_cd.response[0] = 3;
            return;
        }
        ++hm_cd.position;
        hm_cd.destination += 2048;
        --hm_cd.remaining;
    }
    if (!hm_cd.remaining)
        hm_cd.destination = NULL;
}

void hm_cd_tick(void) {
    if (!hm_cd.remaining && !hm_cd.streaming)
        return;
    hm_cd_require("hm_cd_tick");
    hm_cd.fraction += (hm_cd.mode & 0x80) ? 150 : 75;
    u32 budget = hm_cd.fraction / 60;
    hm_cd.fraction %= 60;
    while (hm_cd.streaming && budget) {
        u8 raw[2352];
        if (hm_cd.position >= hm_cd.disc->sectors ||
            !hm_cd.disc->stream.read_at(hm_cd.disc->stream.context, (u64)hm_cd.position * 2352, raw,
                                        sizeof(raw)))
            hm_runtime_error("hm_cd_tick", "cannot read XA sector");
        ++hm_cd.position;
        --budget;
        if (raw[15] != 2 || memcmp(raw + 16, raw + 20, 4))
            hm_runtime_error("hm_cd_tick", "invalid XA sector header");
        if ((hm_cd.mode & 8) && (raw[16] != hm_cd.filter_file || raw[17] != hm_cd.filter_channel))
            continue;
        if ((raw[18] & 0x64) == 0x64) {
            s16 decoded[HM_XA_FRAMES * 2];
            if (!hm_xa_decode_sector(&hm_cd.decoder, raw, decoded))
                hm_runtime_error("hm_cd_tick", "unsupported XA coding or malformed sector");
            if (hm_cd.audio_count + HM_XA_FRAMES > 8192)
                hm_runtime_error("hm_cd_tick", "XA audio queue capacity exceeded");
            for (u32 i = 0; i < HM_XA_FRAMES; ++i) {
                u32 slot = (hm_cd.audio_read + hm_cd.audio_count++) & 8191;
                hm_cd.audio[slot][0] = decoded[i * 2];
                hm_cd.audio[slot][1] = decoded[i * 2 + 1];
            }
        }
        if (raw[18] & 0x80)
            hm_cd.streaming = 0;
    }
    hm_cd_transfer(budget);
}

s32 CdReadSync(s32 mode, u8 *result) {
    hm_cd_require("CdReadSync");
    if (mode != 0 && mode != 1)
        hm_runtime_error("CdReadSync", "unsupported polling mode");
    if (!mode)
        hm_cd_transfer(hm_cd.remaining);
    if (result)
        result[0] = hm_cd.failed ? 3 : 2;
    return hm_cd.failed ? -1 : (s32)hm_cd.remaining;
}

s32 CdSync(s32 mode, u8 *result) {
    hm_cd_require("CdSync");
    if (mode != 0 && mode != 1)
        hm_runtime_error("CdSync", "unsupported polling mode");
    if (result)
        memcpy(result, hm_cd.response, sizeof(hm_cd.response));
    return (hm_cd.failed || hm_cd.command_failed) ? 5 : 2;
}

s32 CdReadBreak(void) {
    hm_cd_require("CdReadBreak");
    hm_cd.remaining = 0;
    hm_cd.destination = NULL;
    hm_cd.failed = 0;
    hm_cd_stop_xa();
    return 1;
}

s32 CdFlush(void) {
    hm_cd_require("CdFlush");
    /* Commands are acknowledged synchronously; there is no queued IRQ response
     * to drain. This does not cancel a transfer (CdReadBreak does). */
    return 1;
}

u8 CdLastCom(void) {
    hm_cd_require("CdLastCom");
    return hm_cd.last_command;
}
