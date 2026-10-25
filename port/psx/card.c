#include "psx/error.h"
#include <libmcrd.h>
#include <libmcx.h>

/* Disconnected-device controller state. No card is advertised until a native
 * persistent store is mounted. File/format operations remain explicit failures
 * in pending.c; this must never claim a successful save or a present card. */
static int hm_card_initialized, hm_card_started, hm_mcx_started;
static int hm_card_pending, hm_mcx_pending;
static s32 hm_card_command, hm_mcx_command;

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
    *result = McErrCardNotExist;
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
    hm_mcx_pending = 1;
    return 1;
}
s32 McxSync(s32 mode, s32 *command, s32 *result) {
    if (!hm_mcx_started)
        hm_runtime_error("McxSync", "PocketStation controller is not started");
    if ((mode != 0 && mode != 1) || !command || !result)
        hm_runtime_error("McxSync", "invalid completion query");
    *command = hm_mcx_command;
    *result = McxErrNoCard;
    if (!hm_mcx_pending)
        return -1;
    hm_mcx_pending = 0;
    return 1;
}
