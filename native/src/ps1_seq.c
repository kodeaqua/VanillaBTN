#include "ps1_seq.h"

#include <string.h>

static unsigned big16(const uint8_t *p) {
    return (unsigned)p[0] * 256u + p[1];
}

static unsigned big24(const uint8_t *p) {
    return big16(p) * 256u + p[2];
}

static bool byte(Ps1Seq *seq, unsigned *value) {
    if (seq == NULL || value == NULL || seq->data == NULL || seq->cursor >= seq->size) return false;
    *value = seq->data[seq->cursor++];
    return true;
}

static bool delta(Ps1Seq *seq) {
    if (seq == NULL || seq->data == NULL) return false;
    uint32_t ticks = 0;
    for (unsigned i = 0; i < 4; ++i) {
        unsigned part;
        if (!byte(seq, &part)) return false;
        ticks = (ticks << 7) | (part & 127u);
        if (part < 128) {
            uint64_t units_per_tick = (uint64_t)seq->tempo * PS1_SPU_SAMPLE_RATE;
            if (ticks > UINT64_MAX / units_per_tick) return false;
            uint64_t duration = (uint64_t)ticks * units_per_tick;
            if (UINT64_MAX - seq->time_units < duration) return false;
            seq->time_units += duration;
            return true;
        }
    }
    return false;
}

bool ps1_seq_open(Ps1Seq *seq, const uint8_t *data, size_t size, Ps1VabId bank, bool repeat) {
    if (seq == NULL) return false;
    memset(seq, 0, sizeof(*seq));
    if (data == NULL || size < 15 || memcmp(data, "pQES", 4) != 0
        || data[4] != 0 || data[5] != 0 || data[6] != 0 || data[7] != 1) return false;
    seq->ppqn = big16(data + 8);
    seq->tempo = seq->initial_tempo = big24(data + 10);
    if (seq->ppqn == 0 || seq->tempo == 0) return false;
    seq->data = data;
    seq->size = size;
    seq->cursor = 15;
    seq->bank = bank;
    seq->repeat = repeat;
    seq->seq_vol_left = 127;
    seq->seq_vol_right = 127;
    for (unsigned i = 0; i < 16; ++i) {
        seq->channels[i] = (Ps1SeqChannel){0,127,64,127};
    }
    seq->active = delta(seq);
    return seq->active;
}

static void note_off(Ps1Seq *seq, Ps1Spu *spu, unsigned channel, unsigned note) {
    if (seq == NULL || spu == NULL || channel >= 16) return;
    for (int v = 0; v < 24; ++v) {
        if (spu->voices[v].vab_id == seq->bank
            && seq->voices[v].channel == channel && seq->voices[v].note == note) {
            ps1_spu_key_off(spu, v);
        }
    }
}

static void sequence_key_off_all(Ps1Seq *seq, Ps1Spu *spu) {
    if (seq == NULL || spu == NULL) return;
    for (unsigned v = 0; v < 24; ++v)
        if (spu->voices[v].vab_id == seq->bank) ps1_spu_key_off(spu, (int)v);
}

static void voice_volume(Ps1Seq *seq, Ps1Spu *spu, unsigned voice) {
    if (seq == NULL || spu == NULL || voice >= 24) return;
    if (seq->bank < 0 || seq->bank >= PS1_SPU_VAB_SLOTS) return;
    Ps1VabSlot *bank = &spu->vabs[seq->bank];
    if (!bank->loaded || bank->header == NULL) return;

    Ps1SeqVoice *record = &seq->voices[voice];
    if (record->channel >= 16 || record->program >= 128) return;
    if (record->tone >= bank->program_tone_counts[record->program]) return;

    uint32_t prog_off = 32u + record->program * 16u;
    if (prog_off + 16u > bank->header_size) return;

    uint32_t tone_off = bank->program_tone_offsets[record->program] + record->tone * 32u;
    if (tone_off + 32u > bank->header_size) return;

    Ps1SeqChannel *channel = &seq->channels[record->channel];
    const uint8_t *program = bank->header + prog_off;
    const uint8_t *tone = bank->header + tone_off;
    unsigned volume = record->velocity * channel->volume * channel->expression / (127u * 127u);
    int pan = (int)channel->pan + program[4] + tone[3] - 128;
    if (pan < 0) pan = 0;
    if (pan > 127) pan = 127;
    volume = volume * program[1] * bank->header[24] / (127u * 127u);
    unsigned left = volume * (unsigned)(127 - pan) * 2u / 127u;
    unsigned right = volume * (unsigned)pan * 2u / 127u;
    if (left > 127) left = 127;
    if (right > 127) right = 127;
    left = left * seq->seq_vol_left / 127u;
    right = right * seq->seq_vol_right / 127u;
    spu->voices[voice].vol_left = (int16_t)(left * tone[2] * 32767u / (127u * 127u));
    spu->voices[voice].vol_right = (int16_t)(right * tone[2] * 32767u / (127u * 127u));
}

void ps1_seq_set_vol(Ps1Seq *seq, Ps1Spu *spu, unsigned voll, unsigned volr) {
    if (seq == NULL) return;
    if (voll > 127) voll = 127;
    if (volr > 127) volr = 127;
    seq->seq_vol_left = voll;
    seq->seq_vol_right = volr;
    if (spu == NULL || seq->bank < 0 || seq->bank >= PS1_SPU_VAB_SLOTS || !spu->vabs[seq->bank].loaded) return;
    for (unsigned v = 0; v < 24; ++v) {
        if (spu->voices[v].active && spu->voices[v].vab_id == seq->bank) {
            voice_volume(seq, spu, v);
        }
    }
}

static void note_on(Ps1Seq *seq, Ps1Spu *spu, unsigned channel, unsigned note, unsigned velocity) {
    if (seq == NULL || spu == NULL || channel >= 16) return;
    if (seq->bank < 0 || seq->bank >= PS1_SPU_VAB_SLOTS) return;
    Ps1VabSlot *bank = &spu->vabs[seq->bank];
    if (!bank->loaded || bank->header == NULL) return;

    unsigned program = seq->channels[channel].program;
    if (program >= 128) return;

    ++seq->note_events;
    for (unsigned t = 0; t < bank->program_tone_counts[program]; ++t) {
        uint32_t tone_off = bank->program_tone_offsets[program] + t * 32u;
        if (tone_off + 32u > bank->header_size) continue;
        const uint8_t *tone = bank->header + tone_off;
        unsigned voice = 24;
        if (note < tone[6] || note > tone[7]) continue;
        for (unsigned v = 0; v < 24; ++v) {
            if (!spu->voices[v].active) { voice = v; break; }
            if (spu->voices[v].vab_id == seq->bank
                && (voice == 24 || seq->voices[v].age < seq->voices[voice].age)) voice = v;
        }
        if (voice == 24) continue;
        ps1_spu_key_on_vab(spu, (int)voice, seq->bank, (int)program, (int)t, (int)note, 0, 127, 127);
        seq->voices[voice] = (Ps1SeqVoice){channel,note,velocity,program,t,seq->note_events};
        voice_volume(seq, spu, voice);
    }
}

static bool event(Ps1Seq *seq, Ps1Spu *spu) {
    if (seq == NULL || spu == NULL) return false;
    unsigned status, a, b, channel;
    if (!byte(seq, &status)) return false;
    if (status < 128) {
        --seq->cursor;
        status = seq->running_status;
    } else if (status < 240) seq->running_status = (uint8_t)status;
    if (status == 255) {
        if (!byte(seq, &a)) return false;
        if (a == 0x51) {
            if (seq->size - seq->cursor < 3) return false;
            /* PS1 SEQ omits the MIDI 03 length byte for tempo events. */
            seq->tempo = big24(seq->data + seq->cursor);
            seq->cursor += 3;
            return seq->tempo != 0;
        }
        if (a != 0x2F) return false;
        if (!seq->repeat) { seq->active = false; sequence_key_off_all(seq, spu); return true; }
        sequence_key_off_all(seq, spu);
        seq->cursor = 15;
        seq->running_status = 0;
        seq->tempo = seq->initial_tempo;
        ++seq->loop_count;
        return true;
    }
    channel = status & 15u;
    if (status < 128 || !byte(seq, &a) || a > 127) return false;
    if ((status & 240u) == 0xC0) { seq->channels[channel].program = a; return true; }
    if ((status & 240u) == 0xD0) return true;
    if (!byte(seq, &b) || b > 127) return false;
    switch (status & 240u) {
    case 0x80: note_off(seq, spu, channel, a); return true;
    case 0x90:
        if (b == 0) note_off(seq, spu, channel, a);
        else note_on(seq, spu, channel, a, b);
        return true;
    case 0xA0: return true;
    case 0xB0:
        if (a == 7) seq->channels[channel].volume = b;
        else if (a == 10) seq->channels[channel].pan = b;
        else if (a == 11) seq->channels[channel].expression = b;
        else if (a == 99 && b == 20) {
            seq->loop_cursor = seq->cursor;
            seq->loop_status = seq->running_status;
            seq->loop_tempo = seq->tempo;
        } else if (a == 99 && b == 30 && seq->repeat && seq->loop_cursor != 0) {
            seq->cursor = seq->loop_cursor;
            seq->running_status = seq->loop_status;
            seq->tempo = seq->loop_tempo;
            ++seq->loop_count;
        } else if (a == 120 || a == 123) {
            for (unsigned v = 0; v < 24; ++v) {
                if (spu->voices[v].vab_id == seq->bank
                    && seq->voices[v].channel == channel) ps1_spu_key_off(spu, (int)v);
            }
        } else if (a == 121) {
            seq->channels[channel].expression = 127;
        }
        for (unsigned v = 0; v < 24; ++v) {
            if (spu->voices[v].active && spu->voices[v].vab_id == seq->bank
                && seq->voices[v].channel == channel) voice_volume(seq, spu, v);
        }
        return true;
    case 0xE0: return true;
    default: return false;
    }
}

void ps1_seq_mix(Ps1Seq *seq, Ps1Spu *spu, int16_t *out, int frames) {
    if (seq == NULL || spu == NULL || out == NULL || frames <= 0 || seq->ppqn == 0) return;
    if (seq->bank < 0 || seq->bank >= PS1_SPU_VAB_SLOTS || !spu->vabs[seq->bank].loaded) {
        seq->failed = true;
        seq->active = false;
    }
    for (int i = 0; i < frames; ++i) {
        unsigned events = 0;
        uint64_t denominator = (uint64_t)seq->ppqn * 1000000u;
        /* Keep the timing origin close to the current sample across long loops. */
        if (seq->active && seq->sample_clock >= PS1_SPU_SAMPLE_RATE) {
            uint64_t origin = (seq->sample_clock - 1) * denominator;
            if (seq->time_units >= origin) {
                seq->time_units -= origin;
            } else {
                seq->time_units = 0;
            }
            seq->sample_clock = 1;
        }
        while (seq->active && seq->time_units <= seq->sample_clock * denominator) {
            /* Malformed zero-time loops must not hang the audio thread. */
            if (++events > 4096 || !event(seq, spu)
                || (seq->active && !delta(seq))) {
                seq->failed = true;
                seq->active = false;
                sequence_key_off_all(seq, spu);
            }
        }
        ps1_spu_mix(spu, out + i * 2, 1, 0);
        ++seq->sample_clock;
    }
}
