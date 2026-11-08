#ifndef VANILLABTN_PS1_SEQ_H
#define VANILLABTN_PS1_SEQ_H

#include "ps1_spu.h"
#include <stdbool.h>

typedef struct Ps1SeqChannel {
    unsigned program, volume, pan, expression;
} Ps1SeqChannel;

typedef struct Ps1SeqVoice {
    unsigned channel, note, velocity, program, tone;
    uint64_t age;
} Ps1SeqVoice;

typedef struct Ps1Seq {
    const uint8_t *data; /* Borrowed until playback stops. */
    size_t size, cursor, loop_cursor;
    unsigned ppqn, tempo, initial_tempo, loop_tempo;
    uint8_t running_status, loop_status;
    uint64_t time_units, sample_clock, note_events, loop_count;
    Ps1VabId bank;
    Ps1SeqChannel channels[16];
    Ps1SeqVoice voices[24];
    bool active, failed, repeat;
} Ps1Seq;

bool ps1_seq_open(Ps1Seq *seq, const uint8_t *data, size_t size, Ps1VabId bank, bool repeat);
/* Schedule events at sample boundaries, then mix the same SPU voices to PCM. */
void ps1_seq_mix(Ps1Seq *seq, Ps1Spu *spu, int16_t *out, int frames);

#endif
