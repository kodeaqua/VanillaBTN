#ifndef VANILLABTN_SEQ_PLAYER_H
#define VANILLABTN_SEQ_PLAYER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Native PS1 SEQ (pQES) sequencer backed by PsyZ VAB and SPU voice calls. */

typedef struct VanillabtnSeqProg {
    uint8_t tones;
    uint8_t mvol;
    uint8_t mpan;
} VanillabtnSeqProg;

typedef struct VanillabtnSeqTone {
    uint8_t prior;
    uint8_t mode;
    uint8_t vol;
    uint8_t pan;
    uint8_t center;
    uint8_t shift;
    uint8_t min;
    uint8_t max;
    uint8_t pbmin;
    uint8_t pbmax;
    int16_t vag;
} VanillabtnSeqTone;

typedef struct VanillabtnSeqBackend {
    int (*get_prog)(short vab_id, short prog, VanillabtnSeqProg *out);
    int (*get_tone)(short vab_id, short prog, short tone, VanillabtnSeqTone *out);
    void (*get_all_keys_status)(char status[24]);
    short (*key_on_v)(short voice, short vab_id, short prog, short tone, short note, short fine,
                      short voll, short volr);
    short (*key_off_v)(short voice);
    short (*set_det_vvol)(short voice, short voll, short volr);
    void (*set_voice_pitch)(int voice, uint16_t pitch);
    void (*reverb_off)(void);
    void (*flush_tick)(void);
    void (*lock_audio)(void);
    void (*unlock_audio)(void);
} VanillabtnSeqBackend;

void vanillabtn_seq_set_backend(const VanillabtnSeqBackend *backend);
void vanillabtn_seq_reset(void);
void vanillabtn_seq_register_vab_mvol(short vab_id, uint8_t mvol);
void vanillabtn_seq_unregister_vab(short vab_id);
void vanillabtn_seq_set_stereo(int stereo);
void vanillabtn_seq_disown_voice(int voice);
void vanillabtn_seq_clear_voices(void);

short vanillabtn_seq_open_sized(const uint8_t *data, size_t size, short vab_id);
void vanillabtn_seq_close(short handle);
void vanillabtn_seq_play(short handle, char play_mode, short loop_count);
void vanillabtn_seq_stop(short handle);
void vanillabtn_seq_pause(short handle);
void vanillabtn_seq_replay(short handle);
void vanillabtn_seq_set_vol(short handle, short voll, short volr);
void vanillabtn_seq_tick(void);

#ifdef __psyz
void vanillabtn_seq_init_psyz(void);
short vanillabtn_seq_vab_open_head(unsigned char *addr, short vab_id);
void vanillabtn_seq_vab_close(short vab_id);
void vanillabtn_seq_all_key_off(short mode);
void vanillabtn_seq_set_stereo_psyz(int stereo);
#endif

#ifdef __cplusplus
}
#endif

#endif /* VANILLABTN_SEQ_PLAYER_H */
