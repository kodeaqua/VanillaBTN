#include "seq_player.h"

/* These assertions also check the optimized build. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int on;
    short vab_id;
    short prog;
    short tone;
    short note;
    short fine;
    short voll;
    short volr;
    short det_l;
    short det_r;
    uint16_t pitch;
    int key_on_count;
    int key_off_count;
} MockVoice;

static MockVoice mock_voices[24];
static char mock_status[24];
static int mock_flushes = 0;
static int mock_reverb_offs = 0;
static int mock_total_key_ons = 0;

static void mock_reset(void) {
    memset(mock_voices, 0, sizeof(mock_voices));
    memset(mock_status, 0, sizeof(mock_status));
    mock_flushes = 0;
    mock_reverb_offs = 0;
    mock_total_key_ons = 0;
}

static int mock_get_prog(short vab_id, short prog, VanillabtnSeqProg *out) {
    if (vab_id != 2) {
        return -1;
    }
    out->mvol = 127;
    out->mpan = 64;
    out->tones = (prog == 1) ? 2 : 1;
    return 0;
}

static int mock_get_tone(short vab_id, short prog, short tone, VanillabtnSeqTone *out) {
    if (vab_id != 2) {
        return -1;
    }
    memset(out, 0, sizeof(*out));
    out->prior = (prog == 1) ? 8 : 4;
    out->mode = 4;
    out->vol = 127;
    out->pan = (prog == 1 && tone == 0) ? 0 : (prog == 1 && tone == 1) ? 127 : 64;
    out->center = 60;
    out->shift = 0;
    out->min = 0;
    out->max = 127;
    out->pbmin = 12;
    out->pbmax = 12;
    out->vag = (int16_t)(tone + 1);
    return 0;
}

static void mock_get_keys(char status[24]) {
    memcpy(status, mock_status, 24);
}

static short mock_key_on_v(short voice, short vab_id, short prog, short tone, short note,
                           short fine, short voll, short volr) {
    MockVoice *mv = &mock_voices[voice];
    mv->on = 1;
    mv->vab_id = vab_id;
    mv->prog = prog;
    mv->tone = tone;
    mv->note = note;
    mv->fine = fine;
    mv->voll = voll;
    mv->volr = volr;
    mv->key_on_count++;
    mock_status[voice] = 1;
    mock_total_key_ons++;
    return voice;
}

static short mock_key_off_v(short voice) {
    mock_voices[voice].on = 0;
    mock_voices[voice].key_off_count++;
    mock_status[voice] = 0;
    return 0;
}

static short mock_set_det_vvol(short voice, short voll, short volr) {
    mock_voices[voice].det_l = voll;
    mock_voices[voice].det_r = volr;
    return 0;
}

static void mock_set_pitch(int voice, uint16_t pitch) {
    mock_voices[voice].pitch = pitch;
}

static void mock_reverb_off(void) {
    mock_reverb_offs++;
}

static void mock_flush_tick(void) {
    mock_flushes++;
}

static const VanillabtnSeqBackend mock_backend = {
    mock_get_prog,
    mock_get_tone,
    mock_get_keys,
    mock_key_on_v,
    mock_key_off_v,
    mock_set_det_vvol,
    mock_set_pitch,
    mock_reverb_off,
    mock_flush_tick,
    NULL,
    NULL,
};

static void test_invalid_headers(void) {
    static const uint8_t bad_magic[16] = {
        'N', 'O', 'P', 'E', 0, 0, 0, 1, 1, 224, 7, 161, 32, 4, 2, 0};
    static const uint8_t bad_ver[16] = {
        'p', 'Q', 'E', 'S', 0, 0, 0, 2, 1, 224, 7, 161, 32, 4, 2, 0};
    static const uint8_t zero_res[16] = {
        'p', 'Q', 'E', 'S', 0, 0, 0, 1, 0, 0, 7, 161, 32, 4, 2, 0};
    static const uint8_t header[16] = {
        'p', 'Q', 'E', 'S', 0, 0, 0, 1, 1, 224, 7, 161, 32, 4, 2, 0};

    mock_reset();
    vanillabtn_seq_reset();
    assert(vanillabtn_seq_open_sized(NULL, 16, 2) == -1);
    assert(vanillabtn_seq_open_sized(bad_magic, sizeof(bad_magic), 2) == -1);
    assert(vanillabtn_seq_open_sized(bad_ver, sizeof(bad_ver), 2) == -1);
    assert(vanillabtn_seq_open_sized(zero_res, sizeof(zero_res), 2) == -1);
    assert(vanillabtn_seq_open_sized(bad_magic, 10, 2) == -1);
    assert(vanillabtn_seq_open_sized(header, 0, 2) == -1);
    assert(vanillabtn_seq_open_sized(header, sizeof(header) - 1, 2) == -1);
    assert(vanillabtn_seq_open_sized(header, sizeof(header), 2) == -1);
}

static void test_playback_multitone_nrpn_loop_and_pitch_bend(void) {
    /* pQES v1, res=60 PPQ, tempo=1,000,000 us (60 BPM -> 1 pulse per 60 Hz tick). */
    static const uint8_t song[] = {
        'p', 'Q', 'E', 'S', 0x00, 0x00, 0x00, 0x01,
        0x00, 0x3C,             /* resolution = 60 */
        0x0F, 0x42, 0x40,       /* tempo = 1,000,000 us */
        0x04, 0x02,             /* rhythm 4/4 */
        0x00, 0xC0, 0x01,       /* t=0: ch0 prog 1 (2 overlapping tones) */
        0x00, 0xB0, 99, 20,     /* t=0: NRPN loop start */
        0x00, 6, 127,           /* t=0: running status CC 6 = 127 (infinite) */
        0x01, 0x90, 60, 100,    /* t=1: ch0 note 60 vel 100 -> keys on 2 voices (21, 20) */
        0x01, 0xB0, 7, 64,      /* t=2: ch0 CC 7 vol = 64 -> updates det_vvol */
        0x01, 0xE0, 0x00, 0x50, /* t=3: ch0 pitch bend up (msb=80 -> +3 semitones) */
        0x01, 0x90, 60, 0,      /* t=4: ch0 note 60 off (running vel=0) */
        0x01, 0xB0, 99, 30,     /* t=5: NRPN loop end -> jump back after CC 6 */
        0x00, 0xFF, 0x2F,       /* end of track (not reached due to NRPN loop) */
    };
    short h;

    mock_reset();
    vanillabtn_seq_reset();
    vanillabtn_seq_register_vab_mvol(2, 100);
    h = vanillabtn_seq_open_sized(song, sizeof(song), 2);
    assert(h == 0);

    /* sound_seq_start sets volume, stops, then plays. */
    vanillabtn_seq_set_vol(h, 100, 100);
    vanillabtn_seq_stop(h);
    assert(mock_reverb_offs == 1);
    vanillabtn_seq_play(h, 1, 0);

    /* Tick 1: pulses=1 -> t=0 setup + t=1 note on (2 tones on voices 21 and 20). */
    vanillabtn_seq_tick();
    assert(mock_flushes == 1);
    assert(mock_voices[21].on == 1 && mock_voices[20].on == 1);
    assert(mock_voices[21].note == 60 && mock_voices[20].note == 60);
    assert(mock_voices[21].voll > 0 && mock_voices[21].volr > 0);
    /* SFX voice 0 stays untouched. */
    assert(mock_voices[0].key_on_count == 0);

    /* Tick 2: CC 7 updates live hardware volume on both voices. */
    vanillabtn_seq_tick();
    assert(mock_voices[21].det_l > 0 && mock_voices[21].det_r == 0);
    assert(mock_voices[20].det_l == 0 && mock_voices[20].det_r > 0);

    /* Tick 3: Pitch bend updates SPU pitch above 0x1000 on both voices. */
    vanillabtn_seq_tick();
    assert(mock_voices[21].pitch > 0x1000 && mock_voices[20].pitch > 0x1000);

    /* Tick 4: Note off releases both voices. */
    vanillabtn_seq_tick();
    assert(mock_voices[21].on == 0 && mock_voices[20].on == 0);

    /* Tick 5: NRPN loop end jumps back to loop start; Tick 6 re-triggers note 60! */
    vanillabtn_seq_tick();
    vanillabtn_seq_tick();
    assert(mock_total_key_ons == 4);
    assert(mock_voices[21].on == 1 && mock_voices[20].on == 1);

    /* SsSeqSetVol(0, 0) mutes live voices immediately. */
    vanillabtn_seq_set_vol(h, 0, 0);
    assert(mock_voices[21].det_l == 0 && mock_voices[21].det_r == 0);
    assert(mock_voices[20].det_l == 0 && mock_voices[20].det_r == 0);

    /* Pause keys off voices; Replay resumes without resetting loop. */
    vanillabtn_seq_pause(h);
    assert(mock_voices[21].on == 0 && mock_voices[20].on == 0);
    vanillabtn_seq_replay(h);
    vanillabtn_seq_close(h);
}

static void test_sfx_coexistence_and_eot_loop(void) {
    /* 1-repeat song ending at 0xFF 0x2F, with SFX busy on voices 0..1. */
    static const uint8_t song[] = {
        'p', 'Q', 'E', 'S', 0x00, 0x00, 0x00, 0x01,
        0x00, 0x3C,
        0x0F, 0x42, 0x40,
        0x04, 0x02,
        0x00, 0xC0, 0x00,
        0x01, 0x90, 64, 127,
        0x01, 0x80, 64, 0,
        0x00, 0xFF, 0x2F,
    };
    short h;

    mock_reset();
    vanillabtn_seq_reset();
    mock_status[0] = 2; /* Active SFX on voice 0. */
    mock_status[1] = 2; /* Active SFX on voice 1. */
    h = vanillabtn_seq_open_sized(song, sizeof(song), 2);
    assert(h == 0);
    vanillabtn_seq_play(h, 1, 2); /* Play exactly 2 times. */

    vanillabtn_seq_tick(); /* t=1: note on (pass 1) */
    assert(mock_total_key_ons == 1);
    assert(mock_voices[21].on == 1);
    assert(mock_status[0] == 2 && mock_status[1] == 2);

    vanillabtn_seq_tick(); /* t=2: note off + EOT -> restarts pass 2 */
    vanillabtn_seq_tick(); /* t=1 (pass 2): note on */
    assert(mock_total_key_ons == 2);

    vanillabtn_seq_tick(); /* t=2 (pass 2): note off + EOT -> stops */
    vanillabtn_seq_tick(); /* idle */
    assert(mock_total_key_ons == 2);
    assert(mock_voices[21].on == 0);
    assert(mock_status[0] == 2 && mock_status[1] == 2);
    vanillabtn_seq_close(h);
}

static void test_truncated_note_off_releases_voice(void) {
    static const uint8_t song[] = {
        'p', 'Q', 'E', 'S', 0, 0, 0, 1, 0, 60, 0x0f, 0x42, 0x40, 4, 2,
        0, 0x90, 64, 127, 1, 0x80, 64,
    };
    mock_reset();
    vanillabtn_seq_reset();
    short h = vanillabtn_seq_open_sized(song, sizeof(song), 2);
    assert(h >= 0);
    vanillabtn_seq_play(h, 1, 1);
    vanillabtn_seq_tick();
    assert(mock_total_key_ons == 1);
    assert(mock_voices[21].key_off_count == 1);
    vanillabtn_seq_close(h);
}

static void test_muted_vab_stays_silent_on_volume_change(void) {
    static const uint8_t song[] = {
        'p', 'Q', 'E', 'S', 0, 0, 0, 1, 0, 60, 0x0f, 0x42, 0x40, 4, 2,
        0, 0x90, 64, 127, 1, 0xb0, 7, 127, 1, 0xff, 0x2f,
    };
    mock_reset();
    vanillabtn_seq_reset();
    vanillabtn_seq_register_vab_mvol(2, 0);
    short h = vanillabtn_seq_open_sized(song, sizeof(song), 2);
    assert(h >= 0);
    vanillabtn_seq_play(h, 1, 1);
    vanillabtn_seq_tick();
    assert(mock_voices[21].on);
    assert(mock_voices[21].det_l == 0 && mock_voices[21].det_r == 0);
    vanillabtn_seq_close(h);
}

static void test_controller_reset_restores_held_note_pitch(void) {
    static const uint8_t song[] = {
        'p', 'Q', 'E', 'S', 0, 0, 0, 1, 0, 60, 0x0f, 0x42, 0x40, 4, 2,
        0, 0x90, 60, 100, 1, 0xe0, 0, 80, 1, 0xb0, 121, 0, 1, 0xff, 0x2f,
    };
    mock_reset();
    vanillabtn_seq_reset();
    short h = vanillabtn_seq_open_sized(song, sizeof(song), 2);
    assert(h == 0);
    vanillabtn_seq_play(h, 1, 1);
    vanillabtn_seq_tick();
    assert(mock_voices[21].pitch > 0x1000);
    vanillabtn_seq_tick();
    assert(mock_voices[21].on && mock_voices[21].pitch == 0x1000);
    vanillabtn_seq_close(h);
}

static void test_loop_keeps_fractional_timing(void) {
    /* 80 pulses per second gives four one-pulse loops in three ticks. */
    static const uint8_t song[] = {
        'p', 'Q', 'E', 'S', 0, 0, 0, 1, 0, 80, 0x0f, 0x42, 0x40, 4, 2,
        1, 0x90, 60, 100, 0, 0x80, 60, 0, 0, 0xff, 0x2f,
    };
    mock_reset();
    vanillabtn_seq_reset();
    short h = vanillabtn_seq_open_sized(song, sizeof(song), 2);
    assert(h == 0);
    vanillabtn_seq_play(h, 1, 0);
    for (int i = 0; i < 3; ++i) {
        vanillabtn_seq_tick();
    }
    assert(mock_total_key_ons == 4);
    vanillabtn_seq_close(h);
}

static void test_tempo_change_applies_within_tick(void) {
    /* One pulse at 120 PPQ uses half a tick. Doubling tempo leaves two pulses. */
    static const uint8_t song[] = {
        'p', 'Q', 'E', 'S', 0, 0, 0, 1, 0, 120, 0x0f, 0x42, 0x40, 4, 2,
        1, 0xff, 0x51, 0x07, 0xa1, 0x20, 2, 0x90, 60, 100, 1, 0xff, 0x2f,
    };
    mock_reset();
    vanillabtn_seq_reset();
    short h = vanillabtn_seq_open_sized(song, sizeof(song), 2);
    assert(h == 0);
    vanillabtn_seq_play(h, 1, 1);
    vanillabtn_seq_tick();
    assert(mock_total_key_ons == 1);
    vanillabtn_seq_close(h);
}

int main(void) {
    vanillabtn_seq_set_backend(&mock_backend);
    test_tempo_change_applies_within_tick();
    test_loop_keeps_fractional_timing();
    test_controller_reset_restores_held_note_pitch();
    test_invalid_headers();
    test_playback_multitone_nrpn_loop_and_pitch_bend();
    test_sfx_coexistence_and_eot_loop();
    test_truncated_note_off_releases_voice();
    test_muted_vab_stays_silent_on_volume_change();
    puts("seq_player unit tests passed.");
    return 0;
}
