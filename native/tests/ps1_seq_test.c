#include "ps1_seq.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#undef assert
#define assert(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

static Ps1Spu spu;
static uint8_t header[4096];
static uint8_t body[16];

static Ps1VabId bank(void) {
    ps1_spu_init(&spu);
    memset(header, 0, sizeof(header));
    memcpy(header, "pBAV", 4);
    header[18] = header[20] = header[22] = 1;
    header[24] = 127; header[25] = 64;
    header[32] = 1; header[33] = 127; header[36] = 64;
    header[0x822] = 127; header[0x823] = 64;
    header[0x824] = 60; header[0x827] = 127;
    /* Fast attack, slow decay to the highest target, frozen sustain and
     * two-clock release: scheduling fixtures must supply real ADSR registers. */
    header[0x830] = 0xff;
    header[0x832] = 0xc0;
    header[0x833] = 0x1f;
    header[0x836] = 1; header[0xA22] = 2;
    memset(body, 0x11, sizeof(body));
    body[0] = 4; body[1] = 7;
    Ps1VabId id = ps1_spu_load_vab(&spu, header, sizeof(header), body, sizeof(body));
    assert(id >= 0);
    return id;
}

static void test_sample_boundaries_and_running_status(void) {
    uint8_t sequence[] = {
        'p','Q','E','S',0,0,0,1,0,100,0x0F,0x42,0x40,4,2,
        0,0xC0,0,
        1,0x90,60,127,
        1,60,0,
        0,0xFF,0x2F,0
    };
    int16_t pcm[885 * 2];
    Ps1Seq seq;
    assert(ps1_seq_open(&seq, sequence, sizeof(sequence), bank(), false));
    ps1_seq_mix(&seq, &spu, pcm, 885);
    for (unsigned i = 0; i < 441; ++i) assert(pcm[i * 2] == 0);
    /* The event starts attack at sample 441; its initial level is zero.
     * Release begins at 882 and reaches zero two envelope clocks later. */
    assert(pcm[441 * 2] == 0 && pcm[442 * 2] > 0 && pcm[881 * 2] > 0);
    assert(pcm[882 * 2] > 0 && pcm[883 * 2] > 0 && pcm[884 * 2] == 0);
    assert(!seq.active && !seq.failed && seq.note_events == 1);
}

static void test_ps1_tempo_and_loop_commands(void) {
    uint8_t sequence[] = {
        'p','Q','E','S',0,0,0,1,0,100,0x0F,0x42,0x40,4,2,
        0,0xFF,0x51,0x07,0xA1,0x20,
        0,0xB0,99,20,
        1,0x90,60,127,
        1,0x90,60,0,
        0,0xB0,99,30,
        0,0xFF,0x2F,0
    };
    int16_t pcm[2000 * 2];
    Ps1Seq seq;
    assert(ps1_seq_open(&seq, sequence, sizeof(sequence), bank(), true));
    ps1_seq_mix(&seq, &spu, pcm, 2000);
    for (unsigned i = 0; i < 221; ++i) assert(pcm[i * 2] == 0);
    assert(pcm[221 * 2] == 0 && pcm[222 * 2] > 0);
    assert(pcm[441 * 2] > 0 && pcm[442 * 2] > 0 && pcm[443 * 2] == 0);
    assert(seq.active && !seq.failed && seq.loop_count >= 4);
}

static void test_malformed_sequences_terminate(void) {
    uint8_t sequence[] = {
        'p','Q','E','S',0,0,0,1,0,100,0x0F,0x42,0x40,4,2,
        0,0xB0,99,20,0,0xB0,99,30
    };
    int16_t pcm[2];
    Ps1Seq seq;
    assert(ps1_seq_open(&seq, sequence, sizeof(sequence), bank(), true));
    ps1_seq_mix(&seq, &spu, pcm, 1);
    assert(seq.failed && !seq.active);
    assert(ps1_seq_open(&seq, sequence, sizeof(sequence), bank(), false));
    seq.cursor = seq.size;
    ps1_seq_mix(&seq, &spu, pcm, 1);
    assert(seq.failed);
    assert(!ps1_seq_open(&seq, sequence, 14, 0, false) && !seq.active);
}

static void test_standard_midi_channel_events(void) {
    uint8_t sequence[] = {
        'p','Q','E','S',0,0,0,1,0,100,0x0F,0x42,0x40,4,2,
        0, 0xB0, 1, 64,      /* CC 1 modulation */
        0, 0xB0, 64, 127,    /* CC 64 sustain */
        0, 0xB0, 121, 0,     /* CC 121 reset all controllers */
        0, 0xA0, 60, 100,    /* Polyphonic aftertouch (2 bytes) */
        0, 0xD0, 80,         /* Channel aftertouch (1 byte) */
        0, 0xE0, 0, 64,      /* Pitch bend (2 bytes) */
        0, 0x90, 60, 127,    /* Note On */
        1, 0x80, 60, 0,      /* Note Off */
        0, 0xFF, 0x2F, 0     /* EOT */
    };
    int16_t pcm[885 * 2];
    Ps1Seq seq;
    assert(ps1_seq_open(&seq, sequence, sizeof(sequence), bank(), false));
    ps1_seq_mix(&seq, &spu, pcm, 885);
    assert(!seq.active && !seq.failed && seq.note_events == 1);
}

int main(void) {
    /* A voice previously used by this sequence may now hold an external effect.
     * Note-off, CC volume/all-notes-off and EOT must not modify that effect. */
    uint8_t sequence[] = {
        'p','Q','E','S',0,0,0,1,0,100,0x0F,0x42,0x40,4,2,
        0,0x80,60,0, 0,0xB0,7,2, 0,0xB0,123,0, 0,0xFF,0x2F,0
    };
    Ps1Seq seq;
    int16_t pcm[2];
    assert(ps1_seq_open(&seq, sequence, sizeof(sequence), bank(), false));
    ps1_spu_key_on_raw(&spu, 0, body, sizeof(body), 0x10000, 12345, 23456);
    ps1_spu_key_on_raw(&spu, 22, body, sizeof(body), 0x10000, 12345, 23456);
    ps1_spu_key_on_raw(&spu, 23, body, sizeof(body), 0x10000, 12345, 23456);
    seq.voices[0].note = 60;
    ps1_seq_mix(&seq, &spu, pcm, 1);
    assert(!seq.active && !seq.failed);
    if (!spu.voices[0].active || !spu.voices[22].active || !spu.voices[23].active) return 20;
    if (spu.voices[0].vol_left != 12345 || spu.voices[0].vol_right != 23456) return 21;
    uint8_t chord[128];
    memcpy(chord, sequence, 15);
    unsigned end = 15;
    for (unsigned note = 0; note < 23; ++note) {
        chord[end++] = 0;
        chord[end++] = 0x90;
        chord[end++] = (uint8_t)(60 + note % 12);
        chord[end++] = 127;
    }
    chord[end++] = 127;
    chord[end++] = 0xFF;
    chord[end++] = 0x2F;
    chord[end++] = 0;
    assert(ps1_seq_open(&seq, chord, end, bank(), false));
    for (unsigned v = 22; v < 24; ++v)
        ps1_spu_key_on_raw(&spu, (int)v, body, sizeof(body), 0x10000, 12345, 23456);
    ps1_seq_mix(&seq, &spu, pcm, 1);
    if (spu.voices[22].vab_id != PS1_VAB_NONE || spu.voices[23].vab_id != PS1_VAB_NONE) return 22;
    assert(seq.note_events == 23 && !seq.failed);
    test_sample_boundaries_and_running_status();
    test_ps1_tempo_and_loop_commands();
    test_malformed_sequences_terminate();
    test_standard_midi_channel_events();
    return 0;
}
