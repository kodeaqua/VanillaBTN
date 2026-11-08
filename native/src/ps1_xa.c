/* ps1_xa.c — host-side PS1 CD-XA stream player and decoder. */

#include "ps1_xa.h"
#include <string.h>

static const int K0[4] = { 0, 60, 115, 98 };
static const int K1[4] = { 0,  0, -52, -55 };

/* 26 CD-XA tracks matching cd_audio.c cd_audio_tracks */
static const Ps1XaTrackEntry s_tracks[PS1_XA_TRACK_COUNT] = {
    {0x0AF2, 0x0000, 0x0000}, {0x0B10, 0x0000, 0x0001}, {0x0108, 0x0000, 0x0002},
    {0x0126, 0x0000, 0x0003}, {0x03EA, 0x0000, 0x0004}, {0x04F8, 0x0000, 0x0005},
    {0x02EE, 0x0000, 0x0006}, {0x003B, 0x0001, 0x0000}, {0x007D, 0x0001, 0x0001},
    {0x009B, 0x0001, 0x0002}, {0x0089, 0x0001, 0x0003}, {0x007D, 0x0001, 0x0004},
    {0x0083, 0x0001, 0x0005}, {0x0089, 0x0001, 0x0006}, {0x0089, 0x0001, 0x0007},
    {0x007D, 0x0002, 0x0000}, {0x0077, 0x0002, 0x0001}, {0x007D, 0x0002, 0x0002},
    {0x007D, 0x0002, 0x0003}, {0x00FB, 0x0003, 0x0000}, {0x00E9, 0x0003, 0x0001},
    {0x01D3, 0x0003, 0x0002}, {0x00E9, 0x0003, 0x0003}, {0x0119, 0x0003, 0x0004},
    {0x00B9, 0x0003, 0x0005}, {0x01D3, 0x0003, 0x0006},
};

const Ps1XaTrackEntry *ps1_xa_get_track_entry(int track) {
    if (track < 0 || track >= PS1_XA_TRACK_COUNT) return NULL;
    return &s_tracks[track];
}

/* -------------------------------------------------------------------------
 * Low-level sector decoding
 * ---------------------------------------------------------------------- */
int ps1_xa_decode_sector(const uint8_t *sector, int16_t *out_samples,
                         int16_t *hist1, int16_t *hist2) {
    if (!sector || !out_samples) return 0;

    /* Verify submode has audio flag (bit 2) */
    uint8_t submode = sector[2];
    if (!(submode & 0x04)) return 0;

    int total_samples = 0;
    int16_t h1 = hist1 ? *hist1 : 0;
    int16_t h2 = hist2 ? *hist2 : 0;

    for (int g = 0; g < PS1_XA_SOUND_GROUPS; g++) {
        const uint8_t *sg = sector + 8 + g * 128;

        for (int b = 0; b < 8; b++) {
            uint8_t param = (b < 4) ? sg[b] : sg[8 + (b - 4)];
            int shift = param & 0x0F;
            int filter = (param >> 4) & 0x03;
            int f0 = K0[filter];
            int f1 = K1[filter];

            for (int s = 0; s < 28; s++) {
                uint8_t w = sg[16 + s * 4 + b / 2];
                uint8_t nib = (b % 2 == 0) ? (w & 0x0F) : ((w >> 4) & 0x0F);
                int16_t sn = (int16_t)(nib << 12) >> shift;
                int32_t pred = (f0 * h1 + f1 * h2 + 32) / 64;
                int32_t sample = sn + pred;

                if (sample > 32767)  sample = 32767;
                if (sample < -32768) sample = -32768;

                h2 = h1;
                h1 = (int16_t)sample;

                out_samples[total_samples++] = (int16_t)sample;
            }
        }
    }

    if (hist1) *hist1 = h1;
    if (hist2) *hist2 = h2;
    return total_samples;
}

/* -------------------------------------------------------------------------
 * Player Lifecycle
 * ---------------------------------------------------------------------- */
void ps1_xa_init(Ps1XaPlayer *player) {
    memset(player, 0, sizeof(*player));
    player->state = PS1_XA_IDLE;
    player->track = -1;
    player->file = -1;
    player->channel = -1;
    player->serial_volume = 45;
    /* 18900 Hz source -> 44100 Hz output */
    player->resample_step = (18900u * 65536u) / 44100u; /* 28087 */
}

void ps1_xa_set_file(Ps1XaPlayer *player, int file_idx, const uint8_t *data, uint32_t size) {
    if (file_idx < 0 || file_idx >= PS1_XA_FILES_COUNT) return;
    player->files[file_idx] = data;
    player->file_sizes[file_idx] = size;
}

static int fetch_next_sector(Ps1XaPlayer *player) {
    if (player->file < 0 || player->file >= PS1_XA_FILES_COUNT) return 0;
    const uint8_t *file_data = player->files[player->file];
    uint32_t file_size = player->file_sizes[player->file];
    if (!file_data) return 0;

    uint32_t num_sectors = file_size / PS1_XA_SECTOR_SIZE;

    while (player->sector_index < num_sectors) {
        const uint8_t *sec = file_data + player->sector_index * PS1_XA_SECTOR_SIZE;
        player->sector_index++;

        /* Check channel filter */
        if (sec[1] == (uint8_t)player->channel) {
            int n = ps1_xa_decode_sector(sec, player->sector_pcm,
                                         &player->hist1, &player->hist2);
            if (n > 0) {
                player->sector_sample_count = n;
                player->sector_sample_idx = 0;
                return 1;
            }
        }
    }

    return 0;
}

static int16_t fetch_xa_raw_sample(Ps1XaPlayer *player) {
    if (player->state == PS1_XA_IDLE) return 0;

    if (player->sector_sample_idx >= player->sector_sample_count) {
        if (!fetch_next_sector(player)) {
            player->state = PS1_XA_IDLE;
            return 0;
        }
    }

    return player->sector_pcm[player->sector_sample_idx++];
}

void ps1_xa_play_track(Ps1XaPlayer *player, int track) {
    if (track < 0 || track >= PS1_XA_TRACK_COUNT) return;

    const Ps1XaTrackEntry *entry = &s_tracks[track];
    player->track = track;
    player->file = entry->file;
    player->channel = entry->channel;
    player->track_volume = entry->volume;
    player->fade_volume = (int16_t)entry->volume;
    player->serial_volume = 45;
    player->sector_index = 0;
    player->sector_sample_count = 0;
    player->sector_sample_idx = 0;
    player->hist1 = 0;
    player->hist2 = 0;
    player->frac_pos = 0;
    player->state = PS1_XA_PLAYING;

    player->curr_sample = fetch_xa_raw_sample(player);
    player->next_sample = fetch_xa_raw_sample(player);
}

void ps1_xa_stop(Ps1XaPlayer *player) {
    player->state = PS1_XA_IDLE;
    player->sector_sample_count = 0;
    player->sector_sample_idx = 0;
}

void ps1_xa_fade_tick(Ps1XaPlayer *player, int speed_mode) {
    if (player->state != PS1_XA_PLAYING && player->state != PS1_XA_FADING) return;

    player->state = PS1_XA_FADING;
    int step = speed_mode;
    if (step < 1) step = 1;
    if (step > 3) step = 3;

    int vol = player->fade_volume - step;
    if (vol <= 0) {
        vol = 0;
        ps1_xa_stop(player);
    }
    player->fade_volume = (int16_t)vol;

    if (vol < 33) {
        player->serial_volume = (int16_t)(vol * 4);
    } else {
        player->serial_volume = 45;
    }
}

Ps1XaState ps1_xa_get_state(const Ps1XaPlayer *player) {
    return player->state;
}

/* -------------------------------------------------------------------------
 * XA Mixer
 * ---------------------------------------------------------------------- */
void ps1_xa_mix(Ps1XaPlayer *player, int16_t *out, int frames,
                int16_t cd_vol_left, int16_t cd_vol_right, int accumulate) {
    if (!player || !out || frames <= 0) return;

    if (player->state == PS1_XA_IDLE) {
        if (!accumulate) {
            memset(out, 0, frames * 2 * sizeof(int16_t));
        }
        return;
    }

    int32_t svol = player->serial_volume; /* 0..128 */
    int32_t cvol_l = cd_vol_left;         /* 0..0x7FFF */
    int32_t cvol_r = cd_vol_right;

    for (int f = 0; f < frames; f++) {
        if (player->state == PS1_XA_IDLE) {
            if (!accumulate) {
                out[f * 2 + 0] = 0;
                out[f * 2 + 1] = 0;
            }
            continue;
        }

        /* Resample with linear interpolation */
        int32_t frac = (int32_t)(player->frac_pos & 0xFFFF);
        int32_t diff = (int32_t)player->next_sample - (int32_t)player->curr_sample;
        int32_t samp = (int32_t)player->curr_sample + ((diff * frac) >> 16);

        player->frac_pos += player->resample_step;
        while (player->frac_pos >= 0x10000 && player->state != PS1_XA_IDLE) {
            player->curr_sample = player->next_sample;
            player->next_sample = fetch_xa_raw_sample(player);
            player->frac_pos -= 0x10000;
        }

        /* Volume scaling: sample * serial_vol (out of 128) * cd_vol (out of 32767) */
        int32_t out_l = (samp * svol * cvol_l) / (128 * 32767);
        int32_t out_r = (samp * svol * cvol_r) / (128 * 32767);

        if (accumulate) {
            out_l += out[f * 2 + 0];
            out_r += out[f * 2 + 1];
        }

        if (out_l >  32767) out_l =  32767;
        if (out_l < -32768) out_l = -32768;
        if (out_r >  32767) out_r =  32767;
        if (out_r < -32768) out_r = -32768;

        out[f * 2 + 0] = (int16_t)out_l;
        out[f * 2 + 1] = (int16_t)out_r;
    }
}
