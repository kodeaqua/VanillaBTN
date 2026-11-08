#include "sdk_pad.h"
#include <libpad.h>
#include <libapi.h>
#include <libetc.h>
#include <string.h>
#include <stdlib.h>

typedef struct Port {
    unsigned char *receive, *actuators;
    unsigned actuator_size;
    uint8_t alignment[6], left_x, left_y;
    uint16_t buttons;
    bool connected, analog, aligned, report_find;
    int ready_at;
} Port;
static Port ports[2];
static bool started;
static void (*rumble)(unsigned, uint8_t, uint8_t, void *);
static void *rumble_context;

static Port *port_at(int port) {
    return port == 0 ? ports : port == 0x10 ? ports + 1 : NULL;
}

static void refresh(unsigned slot) {
    Port *port = ports + slot;
    if (port->receive) {
        unsigned char *data = port->receive;
        data[0] = started && port->connected ? 0 : 0xFF;
        data[1] = port->analog ? 0x73 : 0x41;
        data[2] = (uint8_t)~(port->buttons >> 8);
        data[3] = (uint8_t)~port->buttons;
        data[4] = data[5] = 128;
        data[6] = port->left_x;
        data[7] = port->left_y;
    }
    uint8_t small = 0, large = 0;
    if (started && port->connected && port->aligned && VSync(-1) >= port->ready_at) {
        for (unsigned i = 0; i < 6 && i < port->actuator_size; ++i) {
            if (port->alignment[i] == 0) small = port->actuators[i] != 0;
            if (port->alignment[i] == 1) large = port->actuators[i];
        }
    }
    if (rumble) rumble(slot, small, large, rumble_context);
}

void sdk_pad_sample(unsigned slot, bool connected, bool analog, uint16_t buttons,
    uint8_t left_x, uint8_t left_y) {
    if (slot >= 2) return;
    Port *port = ports + slot;
    if (port->connected != connected || port->analog != analog) {
        port->aligned = false;
        port->report_find = connected;
    }
    port->connected = connected;
    port->analog = analog;
    port->buttons = buttons;
    port->left_x = left_x;
    port->left_y = left_y;
    refresh(slot);
}

void sdk_pad_rumble(void (*send)(unsigned, uint8_t, uint8_t, void *), void *context) {
    rumble = send;
    rumble_context = context;
}

void PadInitDirect(unsigned char *port1, unsigned char *port2) {
    started = false;
    for (unsigned i = 0; i < 2; ++i) {
        ports[i].receive = i ? port2 : port1;
        ports[i].actuators = NULL;
        ports[i].actuator_size = 0;
        ports[i].aligned = false;
        ports[i].report_find = false;
        ports[i].ready_at = 0;
        memset(ports[i].alignment, 0xFF, 6);
        refresh(i);
    }
}

void PadStartCom(void) {
    started = true;
    refresh(0);
    refresh(1);
}

void PadStopCom(void) {
    started = false;
    refresh(0);
    refresh(1);
}

int PadGetState(int number) {
    Port *port = port_at(number);
    if (!port) return PadStateDiscon;
    refresh(number == 0 ? 0 : 1);
    if (!started || !port->connected || !port->receive) return PadStateDiscon;
    /* A hot swap must reach original pad.c's mode-reset path even when the
     * host switches directly between keyboard and a live analog controller. */
    if (port->report_find) { port->report_find = false; return PadStateFindPad; }
    return port->aligned && VSync(-1) < port->ready_at ? PadStateExecCmd : PadStateStable;
}

int PadInfoMode(int number, int query, int index) {
    Port *port = port_at(number);
    if (!port || !started || !port->connected || query != InfoModeCurExID || index != 0) return 0;
    return port->analog ? 7 : 0;
}

void PadSetAct(int number, unsigned char *data, int length) {
    Port *port = port_at(number);
    if (!port || !data || length < 0) return;
    port->actuators = data;
    port->actuator_size = (unsigned)length;
}

int PadSetActAlign(int number, unsigned char *alignment) {
    Port *port = port_at(number);
    if (!port || !alignment || !port->analog || PadGetState(number) != PadStateStable) return 0;
    for (unsigned i = 0; i < 6; ++i)
        if (alignment[i] != 0xFF && alignment[i] > 1) return 0;
    memcpy(port->alignment, alignment, 6);
    port->aligned = true;
    port->ready_at = VSync(-1) + 3;
    return 1;
}

void ChangeClearPAD(long mode) {
    /* This disables a PS1 interrupt-clearing quirk. Native input has no such
     * interrupt source. Only the original game's normal mode is supported. */
    if (mode != 0) abort();
}
