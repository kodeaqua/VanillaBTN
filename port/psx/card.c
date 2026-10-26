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
s32 MemCardExist(s32 port) { return hm_card_request(port, 2); }
s32 MemCardSync(s32 mode, s32 *command, s32 *result) {
    hm_card_require("MemCardSync");
    if ((mode != 0 && mode != 1) || !command || !result)
        hm_runtime_error("MemCardSync", "invalid completion query");
    *command = hm_card_command;
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
        hm_runtime_error("McxSync", "PocketStation controller is not started");
    if ((mode != 0 && mode != 1) || !command || !result)
        hm_runtime_error("McxSync", "invalid completion query");
    *command = hm_mcx_command;
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
