#include "psx/cd_runtime.h"
#include "psx/error.h"
#include "psx/unimplemented.h"
#include <libcd.h>
#include <string.h>

static struct {
    hm_disc *disc;
    u8 *destination;
    u32 position, remaining, fraction;
    u8 mode, last_command, response[8];
    int failed;
} hm_cd;

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
        hm_cd.position = (u32)lba;
        break;
    }
    case CdlSetmode:
        if (!parameters)
            hm_runtime_error("CdControl", "missing mode parameter");
        /* 2340-byte transfers and CDDA sectors need a different destination
         * contract. XA mode can be selected, but starting it still fails below. */
        if (parameters[0] & 0x21)
            hm_runtime_error("CdControl", "unsupported sector transfer mode");
        hm_cd.mode = parameters[0];
        break;
    case CdlStandby:
    case 9: /* Pause */
        hm_cd.remaining = 0;
        hm_cd.destination = NULL;
        break;
    case CdlReadS:
        hm_unimplemented("CdControl(CdlReadS): XA streaming");
    default:
        hm_runtime_error("CdControl", "unsupported CD command");
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
        hm_runtime_error("CdRead", "invalid or overlapping read request");
    if ((u32)sectors > hm_cd.disc->sectors - hm_cd.position)
        hm_runtime_error("CdRead", "read extends beyond the track");
    hm_cd.destination = (u8 *)destination;
    hm_cd.remaining = (u32)sectors;
    hm_cd.mode = (u8)mode;
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
    if (!hm_cd.remaining)
        return;
    hm_cd_require("hm_cd_tick");
    hm_cd.fraction += (hm_cd.mode & 0x80) ? 150 : 75;
    u32 budget = hm_cd.fraction / 60;
    hm_cd.fraction %= 60;
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
    return hm_cd.failed ? 5 : 2;
}

s32 CdReadBreak(void) {
    hm_cd_require("CdReadBreak");
    hm_cd.remaining = 0;
    hm_cd.destination = NULL;
    hm_cd.failed = 0;
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
