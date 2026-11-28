#include "seq_player.h"

#include <string.h>

#ifdef __psyz
#include <libspu.h>
#include <libsnd.h>
#include <psyz/audio.h>
#endif

enum {
    SEQ_MAX_SLOTS = 8,
    SEQ_MAX_VABS = 16,
    SEQ_MIDI_CHANNELS = 16,
    SEQ_MAX_VOICES = 22, /* Matches sound_reserve_voice(0x16) in sound.c. */
    SEQ_TOTAL_VOICES = 24,
    SEQ_MAX_TONES_PER_PROG = 16,
    SEQ_HEADER_SIZE = 15,
    SEQ_MAX_EVENTS_PER_TICK = 256,
    SEQ_TICK_HZ = 60,
    SEQ_US_PER_SEC = 1000000,
    SEQ_VOL_MAX = 127,
    SEQ_PAN_CENTER = 64,
    SEQ_BEND_CENTER = 8192,
    SEQ_HW_VOL_MAX = 16383,
    SEQ_VOL_DENOM = 16129,
    SEQ_PITCH_MAX = 0x3FFF,
    SEQ_DEFAULT_VAB_MVOL = 100,
    SEQ_PLAY_PAUSE = 0,
    SEQ_PLAY_ACTIVE = 1,
    SEQ_NRPN_LOOP_START = 20,
    SEQ_NRPN_LOOP_END = 30,
    SEQ_NRPN_LOOP_INF = 127,
    SEQ_CC_DATA_ENTRY = 6,
    SEQ_CC_MAIN_VOL = 7,
    SEQ_CC_PAN = 10,
    SEQ_CC_EXPRESSION = 11,
    SEQ_CC_DAMPER = 64,
    SEQ_CC_NRPN_LSB = 98,
    SEQ_CC_NRPN_MSB = 99,
    SEQ_CC_RESET_ALL = 121,
    SEQ_META_END_OF_TRACK = 0x2F,
    SEQ_META_SET_TEMPO = 0x51,
};

typedef struct {
    uint8_t program;
    uint8_t vol;
    uint8_t expression;
    uint8_t pan;
    uint8_t damper;
    uint16_t pitch_bend;
    int16_t nrpn_msb;
    int16_t nrpn_lsb;
} SeqChannel;

typedef struct {
    int open;
    int playing;
    int paused;
    short vab_id;
    const uint8_t *data;
    size_t size;
    size_t cursor;
    size_t start_pos;
    size_t loop_pos;
    int has_nrpn_loop;
    uint8_t nrpn_loop_remaining;
    uint8_t loop_running_status;
    uint8_t running_status;
    uint16_t resolution;
    uint32_t initial_tempo_us;
    uint32_t tempo_us;
    uint64_t pulse_num;
    uint32_t delta_remaining;
    short loop_count;
    short loops_done;
    short voll;
    short volr;
    SeqChannel ch[SEQ_MIDI_CHANNELS];
} SeqSlot;

typedef struct {
    int owned;
    int active;
    int released;
    int damper_held;
    short slot;
    short vab_id;
    uint8_t channel;
    uint8_t note;
    uint8_t vel;
    uint8_t prior;
    uint8_t prog_mvol;
    uint8_t prog_mpan;
    uint8_t tone_vol;
    uint8_t tone_pan;
    uint8_t tone_center;
    uint8_t tone_shift;
    uint8_t tone_pbmin;
    uint8_t tone_pbmax;
    uint32_t key_on_tick;
    uint16_t pending_pitch;
} SeqVoice;

/* Exact PS1 libsnd note2pitch table (12 semitones x 16 eighth-semitone steps + octave). */
static const uint16_t seq_pitch_table[193] = {
    0x1000, 0x100E, 0x101D, 0x102C, 0x103B, 0x104A, 0x1059, 0x1068, 0x1078, 0x1087, 0x1096,
    0x10A5, 0x10B5, 0x10C4, 0x10D4, 0x10E3, 0x10F3, 0x1103, 0x1113, 0x1122, 0x1132, 0x1142,
    0x1152, 0x1162, 0x1172, 0x1182, 0x1193, 0x11A3, 0x11B3, 0x11C4, 0x11D4, 0x11E5, 0x11F5,
    0x1206, 0x1216, 0x1227, 0x1238, 0x1249, 0x125A, 0x126B, 0x127C, 0x128D, 0x129E, 0x12AF,
    0x12C1, 0x12D2, 0x12E3, 0x12F5, 0x1306, 0x1318, 0x132A, 0x133C, 0x134D, 0x135F, 0x1371,
    0x1383, 0x1395, 0x13A7, 0x13BA, 0x13CC, 0x13DE, 0x13F1, 0x1403, 0x1416, 0x1428, 0x143B,
    0x144E, 0x1460, 0x1473, 0x1486, 0x1499, 0x14AC, 0x14BF, 0x14D3, 0x14E6, 0x14F9, 0x150D,
    0x1520, 0x1534, 0x1547, 0x155B, 0x156F, 0x1583, 0x1597, 0x15AB, 0x15BF, 0x15D3, 0x15E7,
    0x15FB, 0x1610, 0x1624, 0x1638, 0x164D, 0x1662, 0x1676, 0x168B, 0x16A0, 0x16B5, 0x16CA,
    0x16DF, 0x16F4, 0x170A, 0x171F, 0x1734, 0x174A, 0x175F, 0x1775, 0x178B, 0x17A1, 0x17B6,
    0x17CC, 0x17E2, 0x17F9, 0x180F, 0x1825, 0x183B, 0x1852, 0x1868, 0x187F, 0x1896, 0x18AC,
    0x18C3, 0x18DA, 0x18F1, 0x1908, 0x191F, 0x1937, 0x194E, 0x1965, 0x197D, 0x1995, 0x19AC,
    0x19C4, 0x19DC, 0x19F4, 0x1A0C, 0x1A24, 0x1A3C, 0x1A55, 0x1A6D, 0x1A85, 0x1A9E, 0x1AB7,
    0x1ACF, 0x1AE8, 0x1B01, 0x1B1A, 0x1B33, 0x1B4C, 0x1B66, 0x1B7F, 0x1B98, 0x1BB2, 0x1BCC,
    0x1BE5, 0x1BFF, 0x1C19, 0x1C33, 0x1C4D, 0x1C67, 0x1C82, 0x1C9C, 0x1CB7, 0x1CD1, 0x1CEC,
    0x1D07, 0x1D22, 0x1D3D, 0x1D58, 0x1D73, 0x1D8E, 0x1DA9, 0x1DC5, 0x1DE0, 0x1DFC, 0x1E18,
    0x1E34, 0x1E50, 0x1E6C, 0x1E88, 0x1EA4, 0x1EC1, 0x1EDD, 0x1EFA, 0x1F16, 0x1F33, 0x1F50,
    0x1F6D, 0x1F8A, 0x1FA7, 0x1FC5, 0x1FE2, 0x2000,
};

static SeqSlot seq_slots[SEQ_MAX_SLOTS];
static SeqVoice seq_voices[SEQ_TOTAL_VOICES];
static uint8_t seq_vab_mvol[SEQ_MAX_VABS];
static int seq_stereo_mode = 1;
static uint32_t seq_tick_counter = 1;
static const VanillabtnSeqBackend *seq_backend = NULL;

static void seq_lock(void) {
    if (seq_backend && seq_backend->lock_audio) {
        seq_backend->lock_audio();
    }
}

static void seq_unlock(void) {
    if (seq_backend && seq_backend->unlock_audio) {
        seq_backend->unlock_audio();
    }
}

static short seq_clamp_vol(short vol) {
    if (vol < 0) {
        return 0;
    }
    if (vol > SEQ_VOL_MAX) {
        return SEQ_VOL_MAX;
    }
    return vol;
}

static uint8_t seq_get_vab_mvol(short vab_id) {
    if (vab_id >= 0 && vab_id < SEQ_MAX_VABS) {
        return seq_vab_mvol[vab_id];
    }
    return SEQ_DEFAULT_VAB_MVOL;
}

static void seq_reset_channels(SeqSlot *slot) {
    int c;

    for (c = 0; c < SEQ_MIDI_CHANNELS; c++) {
        slot->ch[c].program = (uint8_t)c;
        slot->ch[c].vol = SEQ_VOL_MAX;
        slot->ch[c].expression = SEQ_VOL_MAX;
        slot->ch[c].pan = SEQ_PAN_CENTER;
        slot->ch[c].damper = 0;
        slot->ch[c].pitch_bend = SEQ_BEND_CENTER;
        slot->ch[c].nrpn_msb = -1;
        slot->ch[c].nrpn_lsb = -1;
    }
}

static int seq_read_vlq(const uint8_t *data, size_t size, size_t *cursor, uint32_t *out) {
    uint32_t value = 0;
    int i;

    for (i = 0; i < 4; i++) {
        uint8_t byte;
        if (*cursor >= size) {
            return 0;
        }
        byte = data[(*cursor)++];
        value = (value << 7) | (uint32_t)(byte & 0x7Fu);
        if ((byte & 0x80u) == 0) {
            *out = value;
            return 1;
        }
    }
    return 0;
}

static void seq_apply_pitch_bend(int base_note, int bend14, uint8_t pbmin, uint8_t pbmax,
                                 int *out_note, int *out_fine) {
    int delta_128 = 0;
    int total_128;

    if (bend14 > SEQ_BEND_CENTER && pbmax > 0) {
        delta_128 = ((bend14 - SEQ_BEND_CENTER) * (int)pbmax * 128) / 8191;
    } else if (bend14 < SEQ_BEND_CENTER && pbmin > 0) {
        delta_128 = -(((SEQ_BEND_CENTER - bend14) * (int)pbmin * 128) / SEQ_BEND_CENTER);
    }
    total_128 = base_note * 128 + delta_128;
    if (total_128 < 0) {
        total_128 = 0;
    } else if (total_128 > 127 * 128 + 127) {
        total_128 = 127 * 128 + 127;
    }
    *out_note = total_128 >> 7;
    *out_fine = total_128 & 127;
}

static uint16_t seq_compute_pitch(int note, int fine, uint8_t center, uint8_t shift_cents) {
    int step = (fine + (int)shift_cents) / 8;
    int octave_carry = 0;
    int semitones;
    int octave;
    int semi;
    int shift;
    uint32_t pitch;

    if (step >= 16) {
        octave_carry = 1;
        step -= 16;
    }
    if (step < 0) {
        step = 0;
    } else if (step > 15) {
        step = 15;
    }
    semitones = note + 60 - (int)center + octave_carry;
    octave = semitones / 12;
    semi = semitones % 12;
    if (semi < 0) {
        semi += 12;
        octave -= 1;
    }
    pitch = seq_pitch_table[semi * 16 + step];
    shift = octave - 5;
    if (shift > 0) {
        pitch = shift >= 16 ? SEQ_PITCH_MAX : (pitch << shift);
    } else if (shift < 0) {
        pitch = (-shift) >= 16 ? 0u : (pitch >> (-shift));
    }
    if (pitch > SEQ_PITCH_MAX) {
        pitch = SEQ_PITCH_MAX;
    }
    return (uint16_t)pitch;
}

static void seq_compute_hw_vol(const SeqSlot *slot, const SeqVoice *voice, short *out_l,
                               short *out_r) {
    const SeqChannel *ch = &slot->ch[voice->channel];
    uint32_t note_vol;
    uint32_t v_scaled;
    uint32_t base;
    uint32_t ch_l;
    uint32_t ch_r;
    uint8_t vab_mvol = seq_get_vab_mvol(voice->vab_id);

    note_vol = ((uint32_t)voice->vel * (uint32_t)ch->vol) / SEQ_VOL_MAX;
    if (ch->expression < SEQ_VOL_MAX) {
        note_vol = (note_vol * (uint32_t)ch->expression) / SEQ_VOL_MAX;
    }
    v_scaled = (note_vol * ((uint32_t)vab_mvol * SEQ_HW_VOL_MAX)) / SEQ_VOL_DENOM;
    base = (v_scaled * (uint32_t)voice->prog_mvol * (uint32_t)voice->tone_vol) / SEQ_VOL_DENOM;
    ch_l = (base * (uint32_t)slot->voll) / SEQ_VOL_MAX;
    ch_r = (base * (uint32_t)slot->volr) / SEQ_VOL_MAX;

    if (voice->tone_pan < SEQ_PAN_CENTER) {
        ch_r = (ch_r * (uint32_t)voice->tone_pan) / 63u;
    } else {
        ch_l = (ch_l * (uint32_t)(SEQ_VOL_MAX - voice->tone_pan)) / 63u;
    }
    if (voice->prog_mpan < SEQ_PAN_CENTER) {
        ch_r = (ch_r * (uint32_t)voice->prog_mpan) / 63u;
    } else {
        ch_l = (ch_l * (uint32_t)(SEQ_VOL_MAX - voice->prog_mpan)) / 63u;
    }
    if (ch->pan < SEQ_PAN_CENTER) {
        ch_r = (ch_r * (uint32_t)ch->pan) / 63u;
    } else {
        ch_l = (ch_l * (uint32_t)(SEQ_VOL_MAX - ch->pan)) / 63u;
    }
    if (!seq_stereo_mode) {
        if (ch_l < ch_r) {
            ch_l = ch_r;
        } else {
            ch_r = ch_l;
        }
    }
    ch_l = (ch_l * ch_l) / SEQ_HW_VOL_MAX;
    ch_r = (ch_r * ch_r) / SEQ_HW_VOL_MAX;
    if (ch_l > SEQ_HW_VOL_MAX) {
        ch_l = SEQ_HW_VOL_MAX;
    }
    if (ch_r > SEQ_HW_VOL_MAX) {
        ch_r = SEQ_HW_VOL_MAX;
    }
    *out_l = (short)ch_l;
    *out_r = (short)ch_r;
}

static void seq_compute_note_on_vol(const SeqSlot *slot, const SeqChannel *ch, uint8_t vel,
                                    short *out_l, short *out_r) {
    uint32_t note_vol = ((uint32_t)vel * (uint32_t)ch->vol) / SEQ_VOL_MAX;
    uint32_t eff_l;
    uint32_t eff_r;

    if (ch->expression < SEQ_VOL_MAX) {
        note_vol = (note_vol * (uint32_t)ch->expression) / SEQ_VOL_MAX;
    }
    eff_l = (note_vol * (uint32_t)slot->voll) / SEQ_VOL_MAX;
    eff_r = (note_vol * (uint32_t)slot->volr) / SEQ_VOL_MAX;
    if (ch->pan < SEQ_PAN_CENTER) {
        eff_r = (eff_r * (uint32_t)ch->pan) / 63u;
    } else {
        eff_l = (eff_l * (uint32_t)(SEQ_VOL_MAX - ch->pan)) / 63u;
    }
    *out_l = seq_clamp_vol((short)eff_l);
    *out_r = seq_clamp_vol((short)eff_r);
}

static void seq_update_slot_voices_vol(short handle, int channel_filter) {
    int v;

    if (!seq_backend || !seq_backend->set_det_vvol) {
        return;
    }
    for (v = 0; v < SEQ_MAX_VOICES; v++) {
        if (!seq_voices[v].owned || seq_voices[v].slot != handle) {
            continue;
        }
        if (channel_filter >= 0 && seq_voices[v].channel != (uint8_t)channel_filter) {
            continue;
        }
        short hw_l;
        short hw_r;
        seq_compute_hw_vol(&seq_slots[handle], &seq_voices[v], &hw_l, &hw_r);
        seq_backend->set_det_vvol((short)v, hw_l, hw_r);
    }
}

static void seq_key_off_slot_voices(short handle) {
    int v;

    for (v = 0; v < SEQ_MAX_VOICES; v++) {
        if (seq_voices[v].owned && seq_voices[v].slot == handle) {
            if (seq_backend && seq_backend->key_off_v) {
                seq_backend->key_off_v((short)v);
            }
            memset(&seq_voices[v], 0, sizeof(seq_voices[v]));
        }
    }
}

static void seq_reap_finished_voices(const char status[SEQ_TOTAL_VOICES]) {
    int v;

    for (v = 0; v < SEQ_MAX_VOICES; v++) {
        if (!seq_voices[v].owned) {
            continue;
        }
        if ((status[v] == 0 || status[v] == 3) &&
            (seq_tick_counter - seq_voices[v].key_on_tick) >= 2u) {
            memset(&seq_voices[v], 0, sizeof(seq_voices[v]));
        }
    }
}

static int seq_alloc_voice(const char status[SEQ_TOTAL_VOICES], uint8_t prior) {
    int v;
    int best_released = -1;
    int best_active = -1;

    /* Scan downward from 21..0 so BGM stays opposite to SFX (which scan 0..23). */
    for (v = SEQ_MAX_VOICES - 1; v >= 0; v--) {
        int recently_keyed =
            seq_voices[v].owned && (seq_tick_counter - seq_voices[v].key_on_tick) < 2u;
        if (!recently_keyed && (status[v] == 0 || status[v] == 3) && !seq_voices[v].active) {
            return v;
        }
    }
    for (v = SEQ_MAX_VOICES - 1; v >= 0; v--) {
        if (!seq_voices[v].owned) {
            continue;
        }
        if (seq_voices[v].released) {
            if (best_released < 0 || seq_voices[v].prior < seq_voices[best_released].prior ||
                (seq_voices[v].prior == seq_voices[best_released].prior &&
                 seq_voices[v].key_on_tick < seq_voices[best_released].key_on_tick)) {
                best_released = v;
            }
        } else if (seq_voices[v].prior <= prior) {
            if (best_active < 0 || seq_voices[v].prior < seq_voices[best_active].prior ||
                (seq_voices[v].prior == seq_voices[best_active].prior &&
                 seq_voices[v].key_on_tick < seq_voices[best_active].key_on_tick)) {
                best_active = v;
            }
        }
    }
    if (best_released >= 0) {
        return best_released;
    }
    return best_active;
}

static void seq_handle_note_off(short handle, uint8_t channel, uint8_t note) {
    SeqSlot *slot = &seq_slots[handle];
    int v;

    for (v = 0; v < SEQ_MAX_VOICES; v++) {
        if (!seq_voices[v].owned || !seq_voices[v].active || seq_voices[v].slot != handle ||
            seq_voices[v].channel != channel || seq_voices[v].note != note) {
            continue;
        }
        if (slot->ch[channel].damper) {
            seq_voices[v].damper_held = 1;
            continue;
        }
        if (seq_backend && seq_backend->key_off_v) {
            seq_backend->key_off_v((short)v);
        }
        seq_voices[v].active = 0;
        seq_voices[v].released = 1;
        seq_voices[v].damper_held = 0;
    }
}

static void seq_handle_note_on(short handle, uint8_t channel, uint8_t note, uint8_t vel,
                               char status[SEQ_TOTAL_VOICES]) {
    SeqSlot *slot = &seq_slots[handle];
    SeqChannel *ch = &slot->ch[channel];
    VanillabtnSeqProg prog;
    int tone_count;
    int t;

    if (vel == 0) {
        seq_handle_note_off(handle, channel, note);
        return;
    }
    if (!seq_backend || !seq_backend->get_prog || !seq_backend->get_tone ||
        !seq_backend->key_on_v) {
        return;
    }
    if (seq_backend->get_prog(slot->vab_id, ch->program, &prog) != 0) {
        return;
    }
    tone_count = prog.tones > SEQ_MAX_TONES_PER_PROG ? SEQ_MAX_TONES_PER_PROG : prog.tones;
    for (t = 0; t < tone_count; t++) {
        VanillabtnSeqTone tone;
        int voice;
        int bent_note;
        int bent_fine;
        short voll;
        short volr;

        if (seq_backend->get_tone(slot->vab_id, ch->program, (short)t, &tone) != 0) {
            continue;
        }
        if (tone.vag <= 0 || tone.vag == 0xFF || note < tone.min || note > tone.max) {
            continue;
        }
        voice = seq_alloc_voice(status, tone.prior);
        if (voice < 0) {
            continue;
        }
        seq_apply_pitch_bend(note, ch->pitch_bend, tone.pbmin, tone.pbmax, &bent_note, &bent_fine);
        seq_compute_note_on_vol(slot, ch, vel, &voll, &volr);
        if (seq_backend->key_on_v((short)voice, slot->vab_id, ch->program, (short)t,
                                  (short)bent_note, (short)bent_fine, voll, volr) < 0) {
            continue;
        }
        status[voice] = 1;
        seq_voices[voice].owned = 1;
        seq_voices[voice].active = 1;
        seq_voices[voice].released = 0;
        seq_voices[voice].damper_held = 0;
        seq_voices[voice].slot = handle;
        seq_voices[voice].vab_id = slot->vab_id;
        seq_voices[voice].channel = channel;
        seq_voices[voice].note = note;
        seq_voices[voice].vel = vel;
        seq_voices[voice].prior = tone.prior;
        seq_voices[voice].prog_mvol = prog.mvol;
        seq_voices[voice].prog_mpan = prog.mpan;
        seq_voices[voice].tone_vol = tone.vol;
        seq_voices[voice].tone_pan = tone.pan;
        seq_voices[voice].tone_center = tone.center;
        seq_voices[voice].tone_shift = tone.shift;
        seq_voices[voice].tone_pbmin = tone.pbmin;
        seq_voices[voice].tone_pbmax = tone.pbmax;
        seq_voices[voice].key_on_tick = seq_tick_counter;
        seq_voices[voice].pending_pitch = 0;
    }
}

static void seq_handle_pitch_bend(short handle, uint8_t channel, uint16_t bend14) {
    SeqSlot *slot = &seq_slots[handle];
    int v;

    slot->ch[channel].pitch_bend = bend14;
    for (v = 0; v < SEQ_MAX_VOICES; v++) {
        int bent_note;
        int bent_fine;

        if (!seq_voices[v].owned || seq_voices[v].slot != handle ||
            seq_voices[v].channel != channel) {
            continue;
        }
        seq_apply_pitch_bend(seq_voices[v].note, bend14, seq_voices[v].tone_pbmin,
                             seq_voices[v].tone_pbmax, &bent_note, &bent_fine);
        seq_voices[v].pending_pitch = seq_compute_pitch(bent_note, bent_fine,
                                                        seq_voices[v].tone_center,
                                                        seq_voices[v].tone_shift);
    }
}

static void seq_handle_damper_off(short handle, uint8_t channel) {
    int v;

    for (v = 0; v < SEQ_MAX_VOICES; v++) {
        if (!seq_voices[v].owned || !seq_voices[v].damper_held || seq_voices[v].slot != handle ||
            seq_voices[v].channel != channel) {
            continue;
        }
        if (seq_backend && seq_backend->key_off_v) {
            seq_backend->key_off_v((short)v);
        }
        seq_voices[v].active = 0;
        seq_voices[v].released = 1;
        seq_voices[v].damper_held = 0;
    }
}

static void seq_handle_control_change(short handle, uint8_t channel, uint8_t cc, uint8_t val) {
    SeqSlot *slot = &seq_slots[handle];
    SeqChannel *ch = &slot->ch[channel];

    switch (cc) {
    case SEQ_CC_MAIN_VOL:
        ch->vol = (uint8_t)seq_clamp_vol(val);
        seq_update_slot_voices_vol(handle, channel);
        break;
    case SEQ_CC_PAN:
        ch->pan = (uint8_t)seq_clamp_vol(val);
        seq_update_slot_voices_vol(handle, channel);
        break;
    case SEQ_CC_EXPRESSION:
        ch->expression = (uint8_t)seq_clamp_vol(val);
        seq_update_slot_voices_vol(handle, channel);
        break;
    case SEQ_CC_DAMPER:
        if (val >= 64) {
            ch->damper = 1;
        } else {
            ch->damper = 0;
            seq_handle_damper_off(handle, channel);
        }
        break;
    case SEQ_CC_NRPN_LSB:
        ch->nrpn_lsb = val;
        break;
    case SEQ_CC_NRPN_MSB:
        ch->nrpn_msb = val;
        if (val == SEQ_NRPN_LOOP_START) {
            slot->loop_pos = slot->cursor;
            slot->loop_running_status = slot->running_status;
            slot->has_nrpn_loop = 1;
            slot->nrpn_loop_remaining = SEQ_NRPN_LOOP_INF;
        } else if (val == SEQ_NRPN_LOOP_END && slot->has_nrpn_loop) {
            if (slot->nrpn_loop_remaining == SEQ_NRPN_LOOP_INF ||
                slot->nrpn_loop_remaining == 0) {
                slot->cursor = slot->loop_pos;
                slot->running_status = slot->loop_running_status;
            } else if (slot->nrpn_loop_remaining > 1) {
                slot->nrpn_loop_remaining--;
                slot->cursor = slot->loop_pos;
                slot->running_status = slot->loop_running_status;
            }
        }
        break;
    case SEQ_CC_DATA_ENTRY:
        if (ch->nrpn_msb == SEQ_NRPN_LOOP_START) {
            slot->loop_pos = slot->cursor;
            slot->loop_running_status = slot->running_status;
            slot->has_nrpn_loop = 1;
            slot->nrpn_loop_remaining = val;
            ch->nrpn_msb = -1;
        }
        break;
    case SEQ_CC_RESET_ALL:
        ch->vol = SEQ_VOL_MAX;
        ch->expression = SEQ_VOL_MAX;
        ch->pan = SEQ_PAN_CENTER;
        seq_handle_pitch_bend(handle, channel, SEQ_BEND_CENTER);
        if (ch->damper) {
            ch->damper = 0;
            seq_handle_damper_off(handle, channel);
        }
        seq_update_slot_voices_vol(handle, channel);
        break;
    default:
        break;
    }
}

static void seq_restart_from_start(SeqSlot *slot) {
    slot->cursor = slot->start_pos;
    slot->loop_pos = slot->start_pos;
    slot->has_nrpn_loop = 0;
    slot->nrpn_loop_remaining = 0;
    slot->loop_running_status = 0;
    slot->running_status = 0;
    slot->tempo_us = slot->initial_tempo_us;
    slot->pulse_num = 0;
    slot->delta_remaining = 0;
    seq_reset_channels(slot);
    if (!seq_read_vlq(slot->data, slot->size, &slot->cursor, &slot->delta_remaining)) {
        slot->playing = 0;
    }
}

static void seq_step_slot(short handle, char status[SEQ_TOTAL_VOICES]) {
    SeqSlot *slot = &seq_slots[handle];
    uint64_t denom;
    uint64_t pulses;
    int events_run = 0;

    if (!slot->open || !slot->playing || slot->paused || slot->resolution == 0 ||
        slot->tempo_us == 0) {
        return;
    }

    denom = (uint64_t)slot->tempo_us * SEQ_TICK_HZ;
    slot->pulse_num += (uint64_t)slot->resolution * SEQ_US_PER_SEC;
    pulses = slot->pulse_num / denom;
    slot->pulse_num %= denom;

    while (slot->playing && events_run < SEQ_MAX_EVENTS_PER_TICK) {
        uint8_t status_byte;
        uint8_t hi;
        uint8_t channel;

        if (slot->delta_remaining > pulses) {
            slot->delta_remaining -= (uint32_t)pulses;
            return;
        }
        pulses -= slot->delta_remaining;
        slot->delta_remaining = 0;

        if (slot->cursor >= slot->size) {
            goto end_track;
        }

        status_byte = slot->data[slot->cursor];
        if (status_byte & 0x80u) {
            slot->cursor++;
            if (status_byte < 0xF0u) {
                slot->running_status = status_byte;
            }
        } else {
            if (slot->running_status == 0) {
                goto end_track;
            }
            status_byte = slot->running_status;
        }

        hi = (uint8_t)(status_byte & 0xF0u);
        channel = (uint8_t)(status_byte & 0x0Fu);
        events_run++;

        if (hi == 0x80u) {
            if (slot->cursor + 2 > slot->size) {
                goto end_track;
            }
            seq_handle_note_off(handle, channel, (uint8_t)(slot->data[slot->cursor] & 0x7Fu));
            slot->cursor += 2;
        } else if (hi == 0x90u) {
            if (slot->cursor + 2 > slot->size) {
                goto end_track;
            }
            seq_handle_note_on(handle, channel, (uint8_t)(slot->data[slot->cursor] & 0x7Fu),
                               (uint8_t)(slot->data[slot->cursor + 1] & 0x7Fu), status);
            slot->cursor += 2;
        } else if (hi == 0xA0u) {
            if (slot->cursor + 2 > slot->size) {
                goto end_track;
            }
            slot->cursor += 2;
        } else if (hi == 0xB0u) {
            uint8_t cc;
            uint8_t val;
            if (slot->cursor + 2 > slot->size) {
                goto end_track;
            }
            cc = (uint8_t)(slot->data[slot->cursor] & 0x7Fu);
            val = (uint8_t)(slot->data[slot->cursor + 1] & 0x7Fu);
            slot->cursor += 2;
            seq_handle_control_change(handle, channel, cc, val);
        } else if (hi == 0xC0u) {
            if (slot->cursor + 1 > slot->size) {
                goto end_track;
            }
            slot->ch[channel].program = (uint8_t)(slot->data[slot->cursor++] & 0x7Fu);
        } else if (hi == 0xD0u) {
            if (slot->cursor + 1 > slot->size) {
                goto end_track;
            }
            slot->cursor += 1;
        } else if (hi == 0xE0u) {
            uint8_t lsb;
            uint8_t msb;
            if (slot->cursor + 2 > slot->size) {
                goto end_track;
            }
            lsb = slot->data[slot->cursor];
            msb = slot->data[slot->cursor + 1];
            slot->cursor += 2;
            seq_handle_pitch_bend(handle, channel, ((msb & 0x7fu) << 7) | (lsb & 0x7fu));
        } else if (status_byte == 0xFFu) {
            uint8_t meta;
            if (slot->cursor >= slot->size) {
                goto end_track;
            }
            meta = slot->data[slot->cursor++];
            if (meta == SEQ_META_END_OF_TRACK) {
                if (slot->loop_count == 0 || slot->loops_done + 1 < slot->loop_count) {
                    if (slot->loop_count > 0) {
                        slot->loops_done++;
                    }
                    seq_restart_from_start(slot);
                    continue;
                }
                goto end_track;
            } else if (meta == SEQ_META_SET_TEMPO) {
                uint32_t new_tempo;
                if (slot->cursor + 3 > slot->size) {
                    goto end_track;
                }
                new_tempo = ((uint32_t)slot->data[slot->cursor] << 16) |
                            ((uint32_t)slot->data[slot->cursor + 1] << 8) |
                            (uint32_t)slot->data[slot->cursor + 2];
                slot->cursor += 3;
                if (new_tempo > 0) {
                    slot->tempo_us = new_tempo;
                    slot->pulse_num = 0;
                }
            } else {
                uint32_t skip_len = 0;
                if (!seq_read_vlq(slot->data, slot->size, &slot->cursor, &skip_len) ||
                    skip_len > slot->size - slot->cursor) {
                    goto end_track;
                }
                slot->cursor += skip_len;
            }
        } else {
            goto end_track;
        }

        if (!seq_read_vlq(slot->data, slot->size, &slot->cursor, &slot->delta_remaining)) {
            goto end_track;
        }
    }
    return;

end_track:
    slot->playing = 0;
    seq_key_off_slot_voices(handle);
}

void vanillabtn_seq_set_backend(const VanillabtnSeqBackend *backend) {
    seq_backend = backend;
}

void vanillabtn_seq_reset(void) {
    int i;

    seq_lock();
    memset(seq_slots, 0, sizeof(seq_slots));
    memset(seq_voices, 0, sizeof(seq_voices));
    for (i = 0; i < SEQ_MAX_VABS; i++) {
        seq_vab_mvol[i] = SEQ_DEFAULT_VAB_MVOL;
    }
    seq_stereo_mode = 1;
    seq_tick_counter = 1;
    seq_unlock();
}

void vanillabtn_seq_register_vab_mvol(short vab_id, uint8_t mvol) {
    if (vab_id >= 0 && vab_id < SEQ_MAX_VABS) {
        seq_lock();
        seq_vab_mvol[vab_id] = mvol > SEQ_VOL_MAX ? SEQ_VOL_MAX : mvol;
        seq_unlock();
    }
}

void vanillabtn_seq_unregister_vab(short vab_id) {
    int v;

    seq_lock();
    for (v = 0; v < SEQ_MAX_VOICES; v++) {
        if (seq_voices[v].owned && seq_voices[v].vab_id == vab_id) {
            if (seq_backend && seq_backend->key_off_v) {
                seq_backend->key_off_v((short)v);
            }
            memset(&seq_voices[v], 0, sizeof(seq_voices[v]));
        }
    }
    if (vab_id >= 0 && vab_id < SEQ_MAX_VABS) {
        seq_vab_mvol[vab_id] = SEQ_DEFAULT_VAB_MVOL;
    }
    seq_unlock();
}

void vanillabtn_seq_set_stereo(int stereo) {
    int i;

    seq_lock();
    seq_stereo_mode = stereo ? 1 : 0;
    for (i = 0; i < SEQ_MAX_SLOTS; i++) {
        if (seq_slots[i].open && seq_slots[i].playing) {
            seq_update_slot_voices_vol((short)i, -1);
        }
    }
    seq_unlock();
}

void vanillabtn_seq_disown_voice(int voice) {
    if (voice >= 0 && voice < SEQ_TOTAL_VOICES) {
        seq_lock();
        memset(&seq_voices[voice], 0, sizeof(seq_voices[voice]));
        seq_unlock();
    }
}

void vanillabtn_seq_clear_voices(void) {
    seq_lock();
    memset(seq_voices, 0, sizeof(seq_voices));
    seq_unlock();
}

short vanillabtn_seq_open_sized(const uint8_t *data, size_t size, short vab_id) {
    int i;
    uint16_t resolution;
    uint32_t tempo_us;
    SeqSlot *slot;

    if (!data || vab_id < 0 || vab_id >= SEQ_MAX_VABS) {
        return -1;
    }
    if (size < SEQ_HEADER_SIZE) {
        return -1;
    }
    if (memcmp(data, "pQES", 4) != 0 || data[7] != 1) {
        return -1;
    }
    resolution = (uint16_t)(((uint16_t)data[8] << 8) | (uint16_t)data[9]);
    tempo_us = ((uint32_t)data[10] << 16) | ((uint32_t)data[11] << 8) | (uint32_t)data[12];
    if (resolution == 0 || tempo_us == 0) {
        return -1;
    }

    seq_lock();
    for (i = 0; i < SEQ_MAX_SLOTS; i++) {
        if (!seq_slots[i].open) {
            break;
        }
    }
    if (i >= SEQ_MAX_SLOTS) {
        seq_unlock();
        return -1;
    }

    slot = &seq_slots[i];
    memset(slot, 0, sizeof(*slot));
    slot->open = 1;
    slot->vab_id = vab_id;
    slot->data = data;
    slot->size = size;
    slot->start_pos = SEQ_HEADER_SIZE;
    slot->resolution = resolution;
    slot->initial_tempo_us = tempo_us;
    slot->tempo_us = tempo_us;
    slot->voll = SEQ_VOL_MAX;
    slot->volr = SEQ_VOL_MAX;
    seq_restart_from_start(slot);
    seq_unlock();
    return (short)i;
}

void vanillabtn_seq_close(short handle) {
    if (handle < 0 || handle >= SEQ_MAX_SLOTS) {
        return;
    }
    seq_lock();
    if (seq_slots[handle].open) {
        seq_key_off_slot_voices(handle);
        memset(&seq_slots[handle], 0, sizeof(seq_slots[handle]));
    }
    seq_unlock();
}

void vanillabtn_seq_play(short handle, char play_mode, short loop_count) {
    SeqSlot *slot;

    if (handle < 0 || handle >= SEQ_MAX_SLOTS) {
        return;
    }
    seq_lock();
    slot = &seq_slots[handle];
    if (!slot->open) {
        seq_unlock();
        return;
    }
    seq_key_off_slot_voices(handle);
    slot->loop_count = loop_count;
    slot->loops_done = 0;
    slot->playing = (play_mode == SEQ_PLAY_ACTIVE);
    slot->paused = (play_mode == SEQ_PLAY_PAUSE);
    seq_restart_from_start(slot);
    if (play_mode != SEQ_PLAY_ACTIVE) {
        slot->playing = 0;
    }
    seq_unlock();
}

void vanillabtn_seq_stop(short handle) {
    SeqSlot *slot;

    if (handle < 0 || handle >= SEQ_MAX_SLOTS) {
        return;
    }
    seq_lock();
    slot = &seq_slots[handle];
    if (!slot->open) {
        seq_unlock();
        return;
    }
    slot->playing = 0;
    slot->paused = 0;
    seq_key_off_slot_voices(handle);
    if (seq_backend && seq_backend->reverb_off) {
        seq_backend->reverb_off();
    }
    /* Keep voll/volr intact; sound_seq_start sets volume before stopping and playing. */
    seq_restart_from_start(slot);
    seq_unlock();
}

void vanillabtn_seq_pause(short handle) {
    if (handle < 0 || handle >= SEQ_MAX_SLOTS) {
        return;
    }
    seq_lock();
    if (seq_slots[handle].open && seq_slots[handle].playing) {
        seq_slots[handle].paused = 1;
        seq_key_off_slot_voices(handle);
    }
    seq_unlock();
}

void vanillabtn_seq_replay(short handle) {
    if (handle < 0 || handle >= SEQ_MAX_SLOTS) {
        return;
    }
    seq_lock();
    if (seq_slots[handle].open && seq_slots[handle].paused) {
        seq_slots[handle].paused = 0;
        seq_slots[handle].playing = 1;
    }
    seq_unlock();
}

void vanillabtn_seq_set_vol(short handle, short voll, short volr) {
    if (handle < 0 || handle >= SEQ_MAX_SLOTS) {
        return;
    }
    seq_lock();
    if (seq_slots[handle].open) {
        seq_slots[handle].voll = seq_clamp_vol(voll);
        seq_slots[handle].volr = seq_clamp_vol(volr);
        seq_update_slot_voices_vol(handle, -1);
    }
    seq_unlock();
}

void vanillabtn_seq_tick(void) {
    char status[SEQ_TOTAL_VOICES];
    int i;
    int v;

    seq_lock();
    seq_tick_counter++;
    memset(status, 0, sizeof(status));
    if (seq_backend && seq_backend->get_all_keys_status) {
        seq_backend->get_all_keys_status(status);
    }
    seq_reap_finished_voices(status);

    for (i = 0; i < SEQ_MAX_SLOTS; i++) {
        if (seq_slots[i].open && seq_slots[i].playing && !seq_slots[i].paused) {
            seq_step_slot((short)i, status);
        }
    }

    if (seq_backend && seq_backend->flush_tick) {
        seq_backend->flush_tick();
    }

    if (seq_backend && seq_backend->set_voice_pitch) {
        for (v = 0; v < SEQ_MAX_VOICES; v++) {
            if (seq_voices[v].owned && seq_voices[v].pending_pitch != 0) {
                seq_backend->set_voice_pitch(v, seq_voices[v].pending_pitch);
                seq_voices[v].pending_pitch = 0;
            }
        }
    }
    seq_unlock();
}

#ifdef __psyz
static int psyz_get_prog(short vab_id, short prog, VanillabtnSeqProg *out) {
    ProgAtr atr;

    if (SsUtGetProgAtr(vab_id, prog, &atr) != 0) {
        return -1;
    }
    out->tones = atr.tones;
    out->mvol = atr.mvol;
    out->mpan = atr.mpan;
    return 0;
}

static int psyz_get_tone(short vab_id, short prog, short tone, VanillabtnSeqTone *out) {
    VagAtr atr;

    if (SsUtGetVagAtr(vab_id, prog, tone, &atr) != 0) {
        return -1;
    }
    out->prior = atr.prior;
    out->mode = atr.mode;
    out->vol = atr.vol;
    out->pan = atr.pan;
    out->center = atr.center;
    out->shift = atr.shift;
    out->min = atr.min;
    out->max = atr.max;
    out->pbmin = atr.pbmin;
    out->pbmax = atr.pbmax;
    out->vag = atr.vag;
    return 0;
}

static void psyz_set_voice_pitch(int voice, uint16_t pitch) {
    SpuSetVoicePitch(voice, pitch);
}

static const VanillabtnSeqBackend psyz_seq_backend = {
    psyz_get_prog,
    psyz_get_tone,
    SpuGetAllKeysStatus,
    SsUtKeyOnV,
    SsUtKeyOffV,
    SsUtSetDetVVol,
    psyz_set_voice_pitch,
    SsUtReverbOff,
    SsSeqCalledTbyT,
    Psyz_AudioLock,
    Psyz_AudioUnlock,
};

void vanillabtn_seq_init_psyz(void) {
    vanillabtn_seq_set_backend(&psyz_seq_backend);
    vanillabtn_seq_reset();
    SsSetTickCallback(vanillabtn_seq_tick);
}

short vanillabtn_seq_vab_open_head(unsigned char *addr, short vab_id) {
    short id = SsVabOpenHead(addr, vab_id);

    if (id >= 0 && addr != NULL) {
        const VabHdr *hdr = (const VabHdr *)addr;
        vanillabtn_seq_register_vab_mvol(id, hdr->mvol);
    }
    return id;
}

void vanillabtn_seq_vab_close(short vab_id) {
    vanillabtn_seq_unregister_vab(vab_id);
    SsVabClose(vab_id);
}

void vanillabtn_seq_all_key_off(short mode) {
    Psyz_AudioLock();
    memset(seq_voices, 0, sizeof(seq_voices));
    SsUtAllKeyOff(mode);
    Psyz_AudioUnlock();
}

void vanillabtn_seq_set_stereo_psyz(int stereo) {
    if (stereo) {
        SsSetStereo();
    } else {
        SsSetMono();
    }
    vanillabtn_seq_set_stereo(stereo);
}
#endif
