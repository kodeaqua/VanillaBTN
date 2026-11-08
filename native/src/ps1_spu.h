/* ps1_spu.h — host-side PS1 SPU compatibility layer.
 *
 * Emulates the 24-voice PlayStation Sound Processing Unit (SPU):
 *   - PS1 ADPCM decode (16-byte blocks -> 28 samples, 5 prediction filters).
 *   - VAB (Voice Archive Body / Sony PSY-Q) header and tone parsing.
 *   - 24 independent voices with pitch/note tuning, volume, and looping.
 *   - Master volume and CD-input mix scaling.
 *   - 44 100 Hz stereo 16-bit PCM output.
 */

#ifndef PS1_SPU_H
#define PS1_SPU_H

#include <stddef.h>
#include <stdint.h>

#define PS1_SPU_VOICES 24
#define PS1_SPU_SAMPLE_RATE 44100
#define PS1_SPU_VAB_SLOTS 8
#define PS1_VAB_NONE (-1)

/* ADPCM block flag bits (byte 1 of 16-byte block) */
#define PS1_ADPCM_FLAG_LOOP_END    0x01
#define PS1_ADPCM_FLAG_LOOP_REPEAT 0x02
#define PS1_ADPCM_FLAG_LOOP_START  0x04

typedef int Ps1VabId;

/* Single voice state */
typedef struct {
    int             active;
    Ps1VabId        vab_id;
    const uint8_t  *sample_data;
    uint32_t        sample_len;
    uint32_t        byte_offset;
    uint32_t        loop_offset;
    int             has_loop;
    int16_t         hist1;
    int16_t         hist2;
    int16_t         block[28];
    int             block_sample_idx; /* 0..27 */
    int16_t         curr_sample;
    int16_t         next_sample;
    uint32_t        frac_pos;         /* 16.16 fractional position */
    uint32_t        pitch_step;       /* 16.16 pitch increment per 44.1kHz sample */
    int16_t         vol_left;         /* 0..0x7FFF */
    int16_t         vol_right;        /* 0..0x7FFF */
} Ps1Voice;

/* Parsed VAB slot */
typedef struct {
    int             loaded;
    const uint8_t  *header;
    uint32_t        header_size;
    const uint8_t  *body;
    uint32_t        body_size;
    uint16_t        num_progs;
    uint16_t        num_tones;
    uint16_t        num_vags;
    uint32_t        vag_body_offsets[256]; /* body byte offset for each VAG 1..255 */
} Ps1VabSlot;

/* SPU state */
typedef struct {
    Ps1Voice        voices[PS1_SPU_VOICES];
    Ps1VabSlot      vabs[PS1_SPU_VAB_SLOTS];
    int16_t         master_vol_left;  /* 0..0x7FFF */
    int16_t         master_vol_right;
    int16_t         cd_vol_left;      /* 0..0x7FFF */
    int16_t         cd_vol_right;
} Ps1Spu;

/* Initialize SPU state */
void ps1_spu_init(Ps1Spu *spu);

/* Decode a single 16-byte PS1 ADPCM block into out[28], updating hist1/hist2.
 * Returns the flags byte (byte 1). */
uint8_t ps1_adpcm_decode_block(const uint8_t *block, int16_t *out,
                                int16_t *hist1, int16_t *hist2);

/* Load a VAB from header and body buffers.
 * If body is NULL, header is assumed to contain both header and body (combined VAB).
 * Returns slot index (0..7) or PS1_VAB_NONE on failure. */
Ps1VabId ps1_spu_load_vab(Ps1Spu *spu, const uint8_t *header, uint32_t header_size,
                          const uint8_t *body, uint32_t body_size);

/* Release a loaded VAB */
void ps1_spu_free_vab(Ps1Spu *spu, Ps1VabId id);

/* Key-on a voice using VAB bank and tone parameters (matches SsUtKeyOnV).
 * note: 0..127 (60 = middle C), fine: -99..99 cents, voll/volr: 0..127. */
void ps1_spu_key_on_vab(Ps1Spu *spu, int voice, Ps1VabId vab_id,
                        int prog, int tone, int note, int fine,
                        int voll, int volr);

/* Key-on raw ADPCM sample on voice with pitch_16_16 (0x10000 = 1.0 = 44.1kHz).
 * vol_left / vol_right: 0..0x7FFF. */
void ps1_spu_key_on_raw(Ps1Spu *spu, int voice, const uint8_t *data,
                        uint32_t data_len, uint32_t pitch_16_16,
                        int16_t vol_left, int16_t vol_right);

/* Key-off voice */
void ps1_spu_key_off(Ps1Spu *spu, int voice);

/* Key-off all voices */
void ps1_spu_key_off_all(Ps1Spu *spu);

/* Check if voice is active */
int ps1_spu_voice_is_active(const Ps1Spu *spu, int voice);

/* Set master volume (0..0x7FFF) */
void ps1_spu_set_master_vol(Ps1Spu *spu, int16_t left, int16_t right);

/* Set CD (XA) mix volume (0..0x7FFF) */
void ps1_spu_set_cd_vol(Ps1Spu *spu, int16_t left, int16_t right);

/* Mix frames stereo 16-bit PCM samples into out (interleaved L/R).
 * If accumulate is non-zero, samples are added to existing contents of out.
 * Otherwise, out is overwritten. */
void ps1_spu_mix(Ps1Spu *spu, int16_t *out, int frames, int accumulate);

#endif /* PS1_SPU_H */
