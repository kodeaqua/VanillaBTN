#include "psx/sequence.h"
#include "psx/error.h"
#include "psx/sound_bank.h"
#include "psx/sound_control.h"
#include <libsnd.h>
#include <string.h>

static int hm_seq_variable(hm_seq_cursor *cursor, u32 *value) {
    *value = 0;
    for (u32 i = 0; i < 4; ++i) {
        if (cursor->at >= cursor->bytes)
            return 0;
        u8 byte = cursor->data[cursor->at++];
        *value = (*value << 7) | (byte & 127);
        if (!(byte & 128))
            return 1;
    }
    return 0;
}

int hm_seq_next(hm_seq_cursor *cursor, hm_seq_event *event) {
    if (!cursor || !event || !cursor->data || cursor->at > cursor->bytes)
        return -1;
    if (cursor->at == cursor->bytes)
        return 0;
    memset(event, 0, sizeof(*event));
    if (!hm_seq_variable(cursor, &event->delta) || cursor->at >= cursor->bytes)
        return -1;
    u8 status = cursor->data[cursor->at];
    if (status & 128)
        ++cursor->at;
    else
        status = cursor->running;
    if (status < 0x80)
        return -1;
    event->status = status;
    if (status < 0xF0) {
        cursor->running = status;
        event->bytes = (status & 0xE0) == 0xC0 ? 1 : 2;
    } else if (status == 0xFF) {
        if (cursor->at >= cursor->bytes)
            return -1;
        event->meta = cursor->data[cursor->at++];
        if (!hm_seq_variable(cursor, &event->bytes))
            return -1;
    } else if (status == 0xF0 || status == 0xF7) {
        cursor->running = 0;
        if (!hm_seq_variable(cursor, &event->bytes))
            return -1;
    } else
        return -1;
    if (event->bytes > cursor->bytes - cursor->at)
        return -1;
    event->payload = cursor->data + cursor->at;
    if (status < 0xF0)
        for (u32 i = 0; i < event->bytes; ++i)
            if (event->payload[i] & 128)
                return -1;
    cursor->at += event->bytes;
    return 1;
}

typedef struct {
    const void *data;
    u32 bytes;
} hm_seq_span;
typedef struct {
    const u8 *data;
    u32 bytes, tempo;
    u16 resolution;
    s32 bank, left, right;
} hm_sequence;
static hm_seq_span hm_spans[16];
static hm_sequence hm_sequences[16];

void hm_sound_sequences_reset(void) {
    memset(hm_spans, 0, sizeof(hm_spans));
    memset(hm_sequences, 0, sizeof(hm_sequences));
}
void hm_sound_sequence_register(const void *data, u32 bytes) {
    if (!data || bytes < 18)
        hm_runtime_error("hm_sound_sequence_register", "invalid asset span");
    for (u32 i = 0; i < 16; ++i)
        if (!hm_spans[i].data || hm_spans[i].data == data) {
            hm_spans[i] = (hm_seq_span){data, bytes};
            return;
        }
    hm_runtime_error("hm_sound_sequence_register", "asset span capacity exceeded");
}

s32 SsSeqOpen(const u32 *data, s32 bank) {
    u32 bytes = 0;
    for (u32 i = 0; i < 16; ++i)
        if (hm_spans[i].data == data)
            bytes = hm_spans[i].bytes;
    if (!bytes)
        hm_runtime_error("SsSeqOpen", "unregistered sequence asset");
    const hm_sound_bank *vab = hm_sound_bank_get(bank);
    if (!vab || vab->uploaded != vab->body_bytes)
        hm_runtime_error("SsSeqOpen", "bank is not loaded");
    const u8 *p = (const u8 *)data;
    if (memcmp(p, "pQES\0\0\0\1", 8))
        hm_runtime_error("SsSeqOpen", "unsupported SEQ signature or version");
    u32 resolution = ((u32)p[8] << 8) | p[9];
    u32 tempo = ((u32)p[10] << 16) | ((u32)p[11] << 8) | p[12];
    if (!resolution || !tempo)
        hm_runtime_error("SsSeqOpen", "invalid sequence timing");
    hm_seq_cursor cursor = {p, bytes, 15, 0};
    hm_seq_event event;
    int ended = 0;
    while (hm_seq_next(&cursor, &event) == 1) {
        if (event.status == 0xFF && event.meta == 0x2F) {
            if (event.bytes)
                hm_runtime_error("SsSeqOpen", "invalid end-of-track event");
            ended = 1;
            break;
        }
    }
    if (!ended)
        hm_runtime_error("SsSeqOpen", "malformed or unterminated sequence");
    s32 limit = hm_sound_configuration()->sequences;
    for (s32 i = 0; i < limit; ++i)
        if (!hm_sequences[i].data) {
            hm_sequences[i] = (hm_sequence){p, cursor.at, tempo, (u16)resolution, bank, 127, 127};
            return i;
        }
    return -1;
}

static hm_sequence *hm_seq_require(s32 sequence, const char *api) {
    if (sequence < 0 || sequence >= 16 || !hm_sequences[sequence].data)
        hm_runtime_error(api, "invalid sequence handle");
    return hm_sequences + sequence;
}
void SsSeqClose(s32 sequence) {
    hm_sequence *state = hm_seq_require(sequence, "SsSeqClose");
    const void *data = state->data;
    memset(state, 0, sizeof(*state));
    for (u32 i = 0; i < 16; ++i)
        if (hm_spans[i].data == data)
            memset(hm_spans + i, 0, sizeof(hm_spans[i]));
}
void SsSeqSetVol(s32 sequence, s32 left, s32 right) {
    hm_sequence *state = hm_seq_require(sequence, "SsSeqSetVol");
    if (left < 0 || left > 127 || right < 0 || right > 127)
        hm_runtime_error("SsSeqSetVol", "invalid volume");
    state->left = left;
    state->right = right;
}

void SsSeqStop(s32 sequence) {
    (void)hm_seq_require(sequence, "SsSeqStop");
    /* Play still aborts, so an opened sequence cannot have active voices.
     * Stopping the actual idle sequence requires no voice release yet. */
}
