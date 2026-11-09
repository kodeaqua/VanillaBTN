/* ps1_xa.h — host-side PS1 CD-XA stream player and decoder.
 *
 * Emulates the CD-ROM XA audio streaming used in Harvest Moon: Back to Nature:
 *   - Form 2 2336-byte sector reading and subheader filtering (file, channel).
 *   - CD-XA ADPCM decode (18 sound groups per sector, 224 samples per group).
 *   - 18.9 kHz / 37.8 kHz mono/stereo resampling to 44.1 kHz stereo PCM.
 *   - Track volume and fade-out matching cd_audio.c semantics.
 */

#ifndef PS1_XA_H
#define PS1_XA_H

#include <stddef.h>
#include <stdint.h>

#define PS1_XA_SECTOR_SIZE 2336
#define PS1_XA_SOUND_GROUPS 18
#define PS1_XA_SAMPLES_PER_GROUP 224
#define PS1_XA_MAX_SECTOR_SAMPLES (PS1_XA_SOUND_GROUPS * PS1_XA_SAMPLES_PER_GROUP) /* 4032 */

#define PS1_XA_TRACK_COUNT 26
#define PS1_XA_FILES_COUNT 4

/* Playback states */
typedef enum {
    PS1_XA_IDLE = 0,
    PS1_XA_REQUESTED = 1,
    PS1_XA_PLAYING = 2,
    PS1_XA_FADING = 3
} Ps1XaState;

/* Track descriptor matching cd_audio_tracks in cd_audio.c */
typedef struct {
    uint16_t volume;
    int16_t  file;
    int16_t  channel;
} Ps1XaTrackEntry;

/* CD-XA Player state */
typedef struct {
    Ps1XaState      state;
    int             track;
    int             file;
    int             channel;
    uint16_t        track_volume;
    int16_t         fade_volume;
    int16_t         serial_volume; /* volume passed to SsSetSerialVol */

    /* Data pointers for the 4 XA files (\\STR\\01.XA .. 04.XA) */
    const uint8_t  *files[PS1_XA_FILES_COUNT];
    uint32_t        file_sizes[PS1_XA_FILES_COUNT];

    /* Sector streaming state */
    uint32_t        sector_index;
    int16_t         sector_pcm[PS1_XA_MAX_SECTOR_SAMPLES];
    int             sector_sample_count;
    int             sector_sample_idx;
    int             is_stereo;
    uint32_t        sample_rate;
    int16_t         curr_sample;
    int16_t         next_sample;
    int16_t         curr_sample_r;
    int16_t         next_sample_r;
    uint32_t        frac_pos;         /* 16.16 */
    uint32_t        resample_step;    /* 16.16 */

    /* ADPCM filter history across sound groups */
    int16_t         hist1;
    int16_t         hist2;
    int16_t         hist1_l;
    int16_t         hist2_l;
    int16_t         hist1_r;
    int16_t         hist2_r;
} Ps1XaPlayer;

/* Initialize player */
void ps1_xa_init(Ps1XaPlayer *player);

/* Set file buffer for stream index 0..3 */
void ps1_xa_set_file(Ps1XaPlayer *player, int file_idx, const uint8_t *data, uint32_t size);

/* Start playing track (0..25) */
void ps1_xa_play_track(Ps1XaPlayer *player, int track);

/* Stop playback */
void ps1_xa_stop(Ps1XaPlayer *player);

/* Tick fade-out (called once per frame, speed_mode 1..3) */
void ps1_xa_fade_tick(Ps1XaPlayer *player, int speed_mode);

/* Get current playback state */
Ps1XaState ps1_xa_get_state(const Ps1XaPlayer *player);

/* Decode a single 2336-byte Form 2 sector in mono.
 * Returns 4032 samples for 4-bit or 2016 for 8-bit coding, or 0 on error. */
int ps1_xa_decode_sector(const uint8_t *sector, int16_t *out_samples,
                         int16_t *hist1, int16_t *hist2);

/* Decode a single 2336-byte Form 2 sector with stereo channel deinterleaving
 * and separate left/right filter histories.
 * Returns 4032 interleaved samples for 4-bit or 2016 for 8-bit coding.
 * Reserved sample widths return 0 without changing output or histories. */
int ps1_xa_decode_sector_stereo(const uint8_t *sector, int16_t *out_samples,
                                int16_t *hist1_l, int16_t *hist2_l,
                                int16_t *hist1_r, int16_t *hist2_r);

/* Mix frames stereo 16-bit PCM at 44.1 kHz into out.
 * cd_vol_left/right are SPU CD-in volumes (0..0x7FFF). */
void ps1_xa_mix(Ps1XaPlayer *player, int16_t *out, int frames,
                int16_t cd_vol_left, int16_t cd_vol_right, int accumulate);

/* Look up track metadata */
const Ps1XaTrackEntry *ps1_xa_get_track_entry(int track);

#endif /* PS1_XA_H */
