#include "psx/sequence.h"
#include "psx/error.h"
#include "psx/sound_bank.h"
#include "psx/sound_control.h"
#include "psx/sound_voice.h"
#include <libsnd.h>
#include <stdio.h>
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
    hm_seq_cursor cursor;
    hm_seq_event next;
    u64 credit;
    u32 initial_tempo, repeats, played;
    hm_seq_cursor loop_cursor;
    u32 loop_tempo, loop_count;
    u8 loop_active, nrpn;
    u8 playing, paused;
    struct {
        u8 program, volume, expression, pan;
        s32 bend;
    } channel[16];
} hm_sequence;
static hm_seq_span hm_spans[16];
static hm_sequence hm_sequences[16];
static struct {
    s32 sequence, channel, note, velocity;
    u64 age;
} hm_seq_owners[24];
static u64 hm_seq_age;

void hm_sound_sequence_forget_voice(u32 voice) {
    if (voice >= 24)
        hm_runtime_error("SEQ voice ownership", "invalid voice");
    hm_seq_owners[voice].sequence = -1;
}
static void hm_seq_release(s32 sequence, s32 channel, s32 note) {
    for (u32 i = 0; i < 24; ++i)
        if (hm_seq_owners[i].sequence == sequence &&
            (channel < 0 || hm_seq_owners[i].channel == channel) &&
            (note < 0 || hm_seq_owners[i].note == note)) {
            SsUtKeyOffV((s32)i);
            hm_sound_sequence_forget_voice(i);
        }
}

void hm_sound_sequences_reset(void) {
    memset(hm_spans, 0, sizeof(hm_spans));
    memset(hm_sequences, 0, sizeof(hm_sequences));
    memset(hm_seq_owners, 0, sizeof(hm_seq_owners));
    for (u32 i = 0; i < 24; ++i)
        hm_sound_sequence_forget_voice(i);
    hm_seq_age = 0;
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
            hm_sequences[i] = (hm_sequence){.data = p,
                                            .bytes = cursor.at,
                                            .tempo = tempo,
                                            .resolution = (u16)resolution,
                                            .bank = bank,
                                            .left = 127,
                                            .right = 127,
                                            .initial_tempo = tempo};
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
    hm_seq_release(sequence, -1, -1);
    const void *data = state->data;
    memset(state, 0, sizeof(*state));
    for (u32 i = 0; i < 16; ++i)
        if (hm_spans[i].data == data)
            memset(hm_spans + i, 0, sizeof(hm_spans[i]));
}
void SsSeqSetVol(s32 sequence, s32 left, s32 right) {
    /* The loader clears every configured slot, including unopened slots. */
    if (sequence < 0 || sequence >= hm_sound_configuration()->sequences)
        hm_runtime_error("SsSeqSetVol", "invalid sequence slot");
    hm_sequence *state = hm_sequences + sequence;
    if (left < 0 || left > 127 || right < 0 || right > 127)
        hm_runtime_error("SsSeqSetVol", "invalid volume");
    state->left = left;
    state->right = right;
    for (u32 i = 0; i < 24; ++i)
        if (hm_seq_owners[i].sequence == sequence) {
            u32 channel = (u32)hm_seq_owners[i].channel;
            s32 volume = hm_seq_owners[i].velocity * state->channel[channel].volume / 127 *
                         state->channel[channel].expression / 127;
            s32 pan = state->channel[channel].pan;
            s32 l = volume * (pan < 64 ? 64 : 127 - pan) / 64;
            s32 r = volume * (pan > 64 ? 64 : pan) / 64;
            if (pan == 64)
                l = r = volume;
            hm_sound_voice_input_volume(i, l * left / 127, r * right / 127);
        }
}

void SsSeqStop(s32 sequence) {
    hm_sequence *state = hm_seq_require(sequence, "SsSeqStop");
    hm_seq_release(sequence, -1, -1);
    state->playing = state->paused = 0;
    state->credit = 0;
}

static void hm_seq_read_next(hm_sequence *state) {
    if (hm_seq_next(&state->cursor, &state->next) != 1)
        hm_runtime_error("SEQ playback", "missing next event");
}
static void hm_seq_rewind(hm_sequence *state) {
    state->cursor = (hm_seq_cursor){state->data, state->bytes, 15, 0};
    state->tempo = state->initial_tempo;
    state->loop_active = state->nrpn = 0;
    for (u32 i = 0; i < 16; ++i) {
        state->channel[i].program = 0;
        state->channel[i].volume = state->channel[i].expression = 127;
        state->channel[i].pan = 64;
        state->channel[i].bend = 0;
    }
    hm_seq_read_next(state);
}
void SsSeqPlay(s32 sequence, s32 mode, s32 repeats) {
    hm_sequence *state = hm_seq_require(sequence, "SsSeqPlay");
    if (mode != 1 || repeats < 0)
        hm_runtime_error("SsSeqPlay", "unsupported play mode or repeat count");
    hm_seq_release(sequence, -1, -1);
    hm_seq_rewind(state);
    state->credit = 0;
    state->repeats = (u32)repeats;
    state->played = 0;
    state->playing = 1;
    state->paused = 0;
}
void SsSeqPause(s32 sequence) {
    hm_sequence *state = hm_seq_require(sequence, "SsSeqPause");
    hm_seq_release(sequence, -1, -1);
    state->paused = 1;
}
void SsSeqReplay(s32 sequence) {
    hm_sequence *state = hm_seq_require(sequence, "SsSeqReplay");
    state->paused = 0;
}

static void hm_seq_note(s32 sequence, u32 channel, u32 note, u32 velocity) {
    hm_sequence *state = hm_sequences + sequence;
    if (!velocity) {
        hm_seq_release(sequence, (s32)channel, (s32)note);
        return;
    }
    u32 program = state->channel[channel].program;
    for (u32 tone = 0; tone < 16; ++tone) {
        const u8 *definition = hm_sound_bank_tone(state->bank, program, tone);
        if (!definition)
            break;
        if (note < definition[6] || note > definition[7])
            continue;
        s32 chosen = -1;
        s32 limit = hm_sound_configuration()->reserved;
        for (s32 voice = 0; voice < limit; ++voice)
            if (!hm_sound_voice_status((u32)voice) || hm_sound_voice_status((u32)voice) == 2) {
                chosen = voice;
                break;
            }
        if (chosen < 0) {
            for (s32 voice = 0; voice < limit; ++voice)
                if (hm_seq_owners[voice].sequence >= 0 &&
                    (chosen < 0 || hm_seq_owners[voice].age < hm_seq_owners[chosen].age))
                    chosen = voice;
        }
        if (chosen < 0)
            continue; /* All allocatable voices belong to external SFX. */
        SsUtKeyOnV(chosen, state->bank, (s32)program, (s32)tone, (s32)note, 0, 127, 127);
        hm_seq_owners[chosen].sequence = sequence;
        hm_seq_owners[chosen].channel = (s32)channel;
        hm_seq_owners[chosen].note = (s32)note;
        hm_seq_owners[chosen].velocity = (s32)velocity;
        hm_seq_owners[chosen].age = ++hm_seq_age;
        hm_sound_voice_bend((u32)chosen, state->channel[channel].bend);
    }
    SsSeqSetVol(sequence, state->left, state->right);
}

static void hm_seq_event_run(s32 sequence) {
    hm_sequence *state = hm_sequences + sequence;
    const hm_seq_event *event = &state->next;
    u32 channel = event->status & 15;
    const u8 *p = event->payload;
    switch (event->status & 0xF0) {
    case 0x80:
        hm_seq_release(sequence, (s32)channel, p[0]);
        break;
    case 0x90:
        hm_seq_note(sequence, channel, p[0], p[1]);
        break;
    case 0xB0:
        switch (p[0]) {
        case 0:
            if (p[1])
                hm_runtime_error("SEQ controller", "nonzero bank select is unsupported");
            break;
        case 7:
            state->channel[channel].volume = p[1];
            break;
        case 10:
            state->channel[channel].pan = p[1];
            break;
        case 11:
            state->channel[channel].expression = p[1];
            break;
        case 99:
            state->nrpn = p[1];
            if (p[1] == 30) {
                if (!state->loop_active)
                    hm_runtime_error("SEQ loop", "end without a loop start");
                if (state->loop_count) {
                    if (state->loop_count != 127)
                        --state->loop_count;
                    state->cursor = state->loop_cursor;
                    state->tempo = state->loop_tempo;
                } else
                    state->loop_active = 0;
            } else if (p[1] != 20)
                hm_runtime_error("SEQ NRPN", "unsupported parameter selector");
            break;
        case 6:
        case 98:
            if (state->nrpn == 20) {
                if (state->loop_active)
                    hm_runtime_error("SEQ loop", "nested loops are unsupported");
                state->loop_cursor = state->cursor;
                state->loop_tempo = state->tempo;
                state->loop_count = p[1];
                state->loop_active = 1;
            } else if (state->nrpn != 30)
                hm_runtime_error("SEQ NRPN", "unsupported data entry");
            break;
        case 123:
            hm_seq_release(sequence, (s32)channel, -1);
            break;
        default: {
            char detail[80];
            snprintf(detail, sizeof(detail), "controller 0x%02x is not implemented", p[0]);
            hm_runtime_error("SEQ controller", detail);
        }
        }
        SsSeqSetVol(sequence, state->left, state->right);
        break;
    case 0xC0:
        state->channel[channel].program = p[0];
        break;
    case 0xE0:
        state->channel[channel].bend = (((s32)p[1] * 128 + p[0]) - 8192) * 254 / 8192;
        for (u32 voice = 0; voice < 24; ++voice)
            if (hm_seq_owners[voice].sequence == sequence &&
                hm_seq_owners[voice].channel == (s32)channel)
                hm_sound_voice_bend(voice, state->channel[channel].bend);
        break;
    case 0xF0:
        if (event->status != 0xFF)
            hm_runtime_error("SEQ playback", "SysEx playback is not implemented");
        if (event->meta == 0x51 && event->bytes == 3) {
            state->tempo = ((u32)p[0] << 16) | ((u32)p[1] << 8) | p[2];
            if (!state->tempo)
                hm_runtime_error("SEQ playback", "zero tempo");
        } else if (event->meta == 0x2F) {
            ++state->played;
            if (state->repeats && state->played >= state->repeats)
                SsSeqStop(sequence);
            else
                hm_seq_rewind(state);
            return;
        } else
            hm_runtime_error("SEQ playback", "unsupported metadata event");
        break;
    default:
        hm_runtime_error("SEQ playback", "unsupported channel pressure event");
    }
    hm_seq_read_next(state);
}

void hm_sound_sequence_tick(void) {
    if (!hm_sound_configuration()->started)
        return;
    for (s32 sequence = 0; sequence < 16; ++sequence) {
        hm_sequence *state = hm_sequences + sequence;
        if (!state->playing || state->paused)
            continue;
        state->credit += (u64)state->resolution * 1000000;
        u32 budget = 65536;
        while (state->playing && !state->paused) {
            u64 due = (u64)state->next.delta * state->tempo * 60;
            if (state->credit < due)
                break;
            if (!budget--)
                hm_runtime_error("SEQ playback", "zero-time event loop");
            state->credit -= due;
            hm_seq_event_run(sequence);
        }
    }
}
