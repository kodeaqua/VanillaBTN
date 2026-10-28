#include "psx/card_store.h"
#include "psx/error.h"
#include <libmcrd.h>
#include <libmcx.h>

/* Game-thread completion state. Host storage is attached separately; without
 * that attachment both slots remain disconnected. */
static int hm_card_initialized, hm_card_started, hm_mcx_started;
static int hm_card_pending, hm_mcx_pending;
static s32 hm_card_command, hm_mcx_command;
static s32 hm_card_result, hm_mcx_result;

static int hm_card_port(s32 port) { return port == 0 || port == 0x10; }
static void hm_card_require(const char *api) {
    if (!hm_card_started)
        hm_runtime_error(api, "memory-card controller is not started");
}

void MemCardInit(s32 mode) {
    if (mode != 0)
        hm_runtime_error("MemCardInit", "unsupported initialization mode");
    hm_card_initialized = 1;
    hm_card_started = hm_mcx_started = hm_card_pending = hm_mcx_pending = 0;
    hm_card_command = hm_mcx_command = 0;
    hm_card_result = McErrCardNotExist;
    hm_mcx_result = McxErrNoCard;
}
void MemCardStart(void) {
    if (!hm_card_initialized)
        hm_runtime_error("MemCardStart", "memory-card controller is not initialized");
    hm_card_started = 1;
}
static s32 hm_card_request(s32 port, s32 command) {
    hm_card_require("memory-card request");
    if (!hm_card_port(port) || hm_card_pending)
        return 0;
    hm_card_command = command; /* Native completion metadata, not a wire opcode. */
    hm_card_result = hm_card_status(port);
    hm_card_pending = 1;
    return 1;
}
s32 MemCardAccept(s32 port) { return hm_card_request(port, 1); }
s32 MemCardExist(s32 port) { return hm_card_request(port, 1); }
s32 MemCardSync(s32 mode, s32 *command, s32 *result) {
    hm_card_require("MemCardSync");
    if (mode != 0 && mode != 1)
        return -1;
    if (command)
        *command = hm_card_command;
    if (result)
        *result = hm_card_result;
    if (!hm_card_pending)
        return -1;
    hm_card_pending = 0;
    return 1;
}
void McxStartCom(void) {
    hm_card_require("McxStartCom");
    hm_mcx_started = 1;
}
s32 McxCardType(s32 port) {
    if (!hm_mcx_started)
        hm_runtime_error("McxCardType", "PocketStation controller is not started");
    if (!hm_card_port(port) || hm_mcx_pending)
        return 0;
    hm_mcx_command = 0x101; /* Opaque native card-type completion ID. */
    /* A native ordinary memory card is not a PocketStation. */
    hm_mcx_result = hm_card_status(port) == McErrCardNotExist ? McxErrNoCard : McxErrInvalid;
    hm_mcx_pending = 1;
    return 1;
}
s32 McxSync(s32 mode, s32 *command, s32 *result) {
    if (!hm_mcx_started)
        return -1;
    if (mode != 0 && mode != 1)
        return -1;
    if (command)
        *command = hm_mcx_command;
    if (result)
        *result = hm_mcx_result;
    if (!hm_mcx_pending)
        return -1;
    hm_mcx_pending = 0;
    return 1;
}

s32 MemCardFormat(s32 port) {
    hm_card_require("MemCardFormat");
    return hm_card_format(port, 1);
}
s32 MemCardUnformat(s32 port) {
    hm_card_require("MemCardUnformat");
    return hm_card_format(port, 0);
}
s32 MemCardCreateFile(s32 port, const char *name, s32 blocks) {
    hm_card_require("MemCardCreateFile");
    return hm_card_create(port, name, blocks);
}
s32 MemCardGetDirentry(s32 port, const char *pattern, struct DIRENTRY *entries, s32 *count,
                       s32 offset, s32 maximum) {
    hm_card_require("MemCardGetDirentry");
    return hm_card_directory(port, pattern, entries, count, offset, maximum);
}
static s32 hm_card_file_request(s32 port, const char *name, void *data, s32 offset, s32 bytes,
                                int write) {
    hm_card_require(write ? "MemCardWriteFile" : "MemCardReadFile");
    if (!hm_card_port(port) || hm_card_pending)
        return 0;
    hm_card_command = write ? 4 : 3; /* The original save overlay polls these IDs. */
    hm_card_result = hm_card_transfer(port, name, data, offset, bytes, write);
    hm_card_pending = 1;
    return 1;
}
s32 MemCardReadFile(s32 port, const char *name, void *data, s32 offset, s32 bytes) {
    return hm_card_file_request(port, name, data, offset, bytes, 0);
}
s32 MemCardWriteFile(s32 port, const char *name, const void *data, s32 offset, s32 bytes) {
    return hm_card_file_request(port, name, (void *)data, offset, bytes, 1);
}

/* Native storage exposes ordinary cards only. These asynchronous requests must
 * complete with a real unsupported-device result, never pretend to transfer
 * PocketStation state or abort just because a frontend probes a device. */
static s32 hm_mcx_ordinary_request(s32 port, s32 command) {
    if (!hm_mcx_started)
        hm_runtime_error("PocketStation request", "controller is not started");
    if (!hm_card_port(port) || hm_mcx_pending)
        return 0;
    hm_mcx_command = command;
    hm_mcx_result = hm_card_status(port) == McErrCardNotExist ? McxErrNoCard : McxErrInvalid;
    hm_mcx_pending = 1;
    return 1;
}
s32 McxAllInfo(s32 port, void *output) {
    (void)output;
    return hm_mcx_ordinary_request(port, 0x102);
}

s32 McxCurrCtrl(s32 port, s32 sound, s32 infrared, s32 unused) {
    (void)sound;
    (void)infrared;
    (void)unused;
    return hm_mcx_ordinary_request(port, 0x103);
}

s32 McxExecApl(s32 port, s32 application, u32 offset) {
    (void)application;
    (void)offset;
    return hm_mcx_ordinary_request(port, 0x104);
}

s32 McxExecFlag(s32 port, s32 block, s32 flag) {
    (void)block;
    (void)flag;
    return hm_mcx_ordinary_request(port, McxFuncExecFlag);
}

s32 McxFlashAcs(s32 port, s32 flag) {
    (void)flag;
    return hm_mcx_ordinary_request(port, 0x106);
}

s32 McxGetApl(s32 port, void *output) {
    (void)output;
    return hm_mcx_ordinary_request(port, 0x107);
}

s32 McxGetMem(s32 port, void *output, u32 offset, u32 bytes) {
    (void)output;
    (void)offset;
    (void)bytes;
    return hm_mcx_ordinary_request(port, 0x108);
}

s32 McxGetSerial(s32 port, void *output) {
    (void)output;
    return hm_mcx_ordinary_request(port, 0x109);
}

s32 McxGetTime(s32 port, void *output) {
    (void)output;
    return hm_mcx_ordinary_request(port, 0x10a);
}

s32 McxHideTrans(s32 port) { return hm_mcx_ordinary_request(port, 0x10b); }

s32 McxReadDev(s32 port, s32 device, const u8 *fixed, u8 *data) {
    (void)device;
    (void)fixed;
    (void)data;
    return hm_mcx_ordinary_request(port, 0x10c);
}

s32 McxSetLED(s32 port, s32 flag) {
    (void)flag;
    return hm_mcx_ordinary_request(port, 0x10d);
}

s32 McxSetMem(s32 port, const void *input, u32 offset, u32 bytes) {
    (void)input;
    (void)offset;
    (void)bytes;
    return hm_mcx_ordinary_request(port, 0x10e);
}

s32 McxShowTrans(s32 port, s32 shown, s32 timeout) {
    (void)shown;
    (void)timeout;
    return hm_mcx_ordinary_request(port, 0x10f);
}

s32 McxWriteDev(s32 port, s32 device, const u8 *fixed, const u8 *data) {
    (void)device;
    (void)fixed;
    (void)data;
    return hm_mcx_ordinary_request(port, 0x110);
}
