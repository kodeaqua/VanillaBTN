#include "psx/pad.h"
#include "psx/error.h"
#include <libapi.h>
#include <libpad.h>

static struct {
    u8 *reply, *actuators;
    u16 held;
    int connected;
} hm_pads[2];
static int hm_pad_started;

static u32 hm_pad_index(s32 port) {
    if (port != 0 && port != 0x10)
        hm_runtime_error("pad port", "invalid pad port");
    return port == 0x10;
}

static void hm_pad_publish(u32 index) {
    u8 *p = hm_pads[index].reply;
    if (!p)
        return;
    p[0] = hm_pad_started && hm_pads[index].connected ? 0 : 0xFF;
    p[1] = 0x41; /* Digital controller ID 4, one 16-bit data word. */
    p[2] = (u8) ~(hm_pads[index].held >> 8);
    p[3] = (u8)~hm_pads[index].held;
    p[4] = p[5] = p[6] = p[7] = 128;
}

void hm_pad_sample(s32 port, int connected, u16 held) {
    u32 index = hm_pad_index(port);
    hm_pads[index].connected = connected != 0;
    hm_pads[index].held = connected ? held : 0;
    hm_pad_publish(index);
}

void PadInitDirect(u8 *first, u8 *second) {
    if (!first || !second)
        hm_runtime_error("PadInitDirect", "null receive buffer");
    hm_pads[0].reply = first;
    hm_pads[1].reply = second;
    hm_pad_started = 0;
    hm_pad_publish(0);
    hm_pad_publish(1);
}
void PadSetAct(s32 port, u8 *actuators, s32 bytes) {
    if (!actuators || bytes != 8)
        hm_runtime_error("PadSetAct", "unsupported actuator buffer");
    hm_pads[hm_pad_index(port)].actuators = actuators;
}
void PadStartCom(void) {
    if (!hm_pads[0].reply || !hm_pads[1].reply)
        hm_runtime_error("PadStartCom", "receive buffers are not initialized");
    hm_pad_started = 1;
    hm_pad_publish(0);
    hm_pad_publish(1);
}
s32 PadGetState(s32 port) {
    u32 index = hm_pad_index(port);
    hm_pad_publish(index);
    return hm_pad_started && hm_pads[index].connected ? PadStateStable : PadStateDiscon;
}
s32 PadInfoMode(s32 port, s32 information, s32 index) {
    if (information != InfoModeCurExID || index != 0)
        hm_runtime_error("PadInfoMode", "unsupported mode information");
    return PadGetState(port) == PadStateStable ? 4 : 0;
}
s32 PadSetActAlign(s32 port, const u8 *alignment) {
    (void)hm_pad_index(port);
    if (!alignment)
        hm_runtime_error("PadSetActAlign", "null actuator alignment");
    /* A digital keyboard/controller has no actuators. Do not acknowledge
     * rumble alignment or pretend that a motor request was played. */
    return 0;
}
s32 ChangeClearPAD(s32 mode) {
    if (mode != 0)
        hm_runtime_error("ChangeClearPAD", "unsupported interrupt-clear mode");
    /* There is no hardware pad ISR in the native single-threaded runtime. */
    return 0;
}
