#include "ps1_seq.h"

#include <assert.h>
#include <string.h>

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
    int16_t pcm[883 * 2];
    Ps1Seq seq;
    assert(ps1_seq_open(&seq, sequence, sizeof(sequence), bank(), false));
    ps1_seq_mix(&seq, &spu, pcm, 883);
    for (unsigned i = 0; i < 441; ++i) assert(pcm[i * 2] == 0);
    assert(pcm[441 * 2] > 0 && pcm[881 * 2] > 0);
    assert(pcm[882 * 2] == 0);
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
    assert(pcm[221 * 2] > 0 && pcm[441 * 2] == 0);
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

int main(void) {
    test_sample_boundaries_and_running_status();
    test_ps1_tempo_and_loop_commands();
    test_malformed_sequences_terminate();
    return 0;
}
