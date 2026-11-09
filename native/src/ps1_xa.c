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
/* Parameters live at bytes 4..11; the surrounding bytes are repair copies.
 * Keep independent channel predictors and round negative predictions with SAR,
 * as specified by the CD-XA ADPCM format. Reserved shifts behave as shift 9. */
static int decode_sector(const uint8_t *sector, int16_t *out_samples,
                         int16_t *hist1_l, int16_t *hist2_l,
                         int16_t *hist1_r, int16_t *hist2_r, int stereo) {
    if (!sector || !out_samples || !(sector[2] & 0x04)) return 0;
    int width = (sector[3] >> 4) & 3;
    if (width > 1) return 0;
    int units = width ? 4 : 8;
    int16_t h1[2] = {hist1_l ? *hist1_l : 0, hist1_r ? *hist1_r : 0};
    int16_t h2[2] = {hist2_l ? *hist2_l : 0, hist2_r ? *hist2_r : 0};

    for (int g = 0; g < PS1_XA_SOUND_GROUPS; g++) {
        const uint8_t *sg = sector + 8 + g * 128;
        for (int unit = 0; unit < units; unit++) {
            int param = sg[4 + unit];
            int shift = param & 15;
            if (shift > 12) shift = 9;
            int filter = (param >> 4) & 3;
            int channel = stereo ? unit & 1 : 0;
            for (int sample = 0; sample < 28; sample++) {
                int byte = sg[16 + sample * 4 + (width ? unit : unit / 2)];
                int raw;
                if (width) {
                    raw = (int8_t)byte * 256;
                } else {
                    int nibble = (byte >> ((unit & 1) * 4)) & 15;
                    raw = (nibble >= 8 ? nibble - 16 : nibble) * 4096;
                }
                int value = (raw >> shift) +
                    ((K0[filter] * h1[channel] + K1[filter] * h2[channel] + 32) >> 6);
                if (value > 32767) value = 32767;
                if (value < -32768) value = -32768;
                h2[channel] = h1[channel];
                h1[channel] = (int16_t)value;
                int index = stereo ? ((unit / 2 * 28 + sample) * 2 + channel)
                                   : unit * 28 + sample;
                out_samples[g * units * 28 + index] = (int16_t)value;
            }
        }
    }
    if (hist1_l) *hist1_l = h1[0];
    if (hist2_l) *hist2_l = h2[0];
    if (hist1_r) *hist1_r = h1[1];
    if (hist2_r) *hist2_r = h2[1];
    return PS1_XA_SOUND_GROUPS * units * 28;
}

int ps1_xa_decode_sector(const uint8_t *sector, int16_t *out_samples,
                         int16_t *hist1, int16_t *hist2) {
    return decode_sector(sector, out_samples, hist1, hist2, NULL, NULL, 0);
}

int ps1_xa_decode_sector_stereo(const uint8_t *sector, int16_t *out_samples,
                                int16_t *hist1_l, int16_t *hist2_l,
                                int16_t *hist1_r, int16_t *hist2_r) {
    return decode_sector(sector, out_samples, hist1_l, hist2_l, hist1_r, hist2_r, 1);
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
    player->filter_file = -1;
    player->serial_volume = 45;
    player->sample_rate = 18900;
    /* 18900 Hz source -> 44100 Hz output */
    player->resample_step = (18900u * 65536u) / 44100u; /* 28087 */
}

void ps1_xa_set_file(Ps1XaPlayer *player, int file_idx, const uint8_t *data, uint32_t size) {
    if (file_idx < 0 || file_idx >= PS1_XA_FILES_COUNT) return;
    player->files[file_idx] = data;
    player->file_sizes[file_idx] = size;
}

static int fetch_next_sector(Ps1XaPlayer *player) {
    const uint8_t *file_data = NULL;
    uint32_t num_sectors = player->reader_sectors;
    if (!player->reader) {
        if (player->file < 0 || player->file >= PS1_XA_FILES_COUNT) return 0;
        file_data = player->files[player->file];
        if (!file_data) return 0;
        num_sectors = player->file_sizes[player->file] / PS1_XA_SECTOR_SIZE;
    }
    uint8_t sector[PS1_XA_SECTOR_SIZE];
    while (player->sector_index < num_sectors) {
        uint32_t index = player->sector_index++;
        const uint8_t *sec;
        if (player->reader) {
            if (!player->reader(player->reader_context, index, sector)) return 0;
            sec = sector;
        } else {
            sec = file_data + index * PS1_XA_SECTOR_SIZE;
        }
        /* Invalid/non-audio subheaders must not change the current format. */
        if (sec[1] == (uint8_t)player->channel && (sec[2] & 0x64) == 0x64
            && (player->filter_file < 0 || sec[0] == player->filter_file)
            && (sec[3] & 0x22) == 0 && ((sec[3] >> 4) & 3) <= 1) {
            uint8_t coding_info = sec[3];
            player->is_stereo = (coding_info & 0x01) != 0;
            player->sample_rate = ((coding_info >> 2) & 0x01) ? 18900u : 37800u;
            player->resample_step = (player->sample_rate * 65536u) / 44100u;

            int n;
            if (player->is_stereo) {
                n = ps1_xa_decode_sector_stereo(sec, player->sector_pcm,
                                               &player->hist1_l, &player->hist2_l,
                                               &player->hist1_r, &player->hist2_r);
                player->hist1 = player->hist1_l;
                player->hist2 = player->hist2_l;
            } else {
                n = ps1_xa_decode_sector(sec, player->sector_pcm,
                                         &player->hist1_l, &player->hist2_l);
                player->hist1 = player->hist1_l;
                player->hist2 = player->hist2_l;
            }
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

static void prime_stream(Ps1XaPlayer *player) {
    player->curr_sample = fetch_xa_raw_sample(player);
    if (player->is_stereo) {
        player->curr_sample_r = fetch_xa_raw_sample(player);
        player->next_sample = fetch_xa_raw_sample(player);
        player->next_sample_r = fetch_xa_raw_sample(player);
    } else {
        player->next_sample = fetch_xa_raw_sample(player);
    }
}

void ps1_xa_play_source(Ps1XaPlayer *player, Ps1XaSectorReader reader, void *context,
                        uint32_t first, uint32_t count, int file_number, int channel) {
    ps1_xa_init(player);
    if (!reader || first >= count || file_number < 0 || file_number > 255
        || channel < 0 || channel > 31) return;
    player->reader = reader;
    player->reader_context = context;
    player->reader_sectors = count;
    player->filter_file = file_number;
    player->channel = channel;
    player->sector_index = first;
    player->serial_volume = 128; /* SDK applies the actual SPU CD-input volume. */
    player->state = PS1_XA_PLAYING;
    prime_stream(player);
}

void ps1_xa_play_track(Ps1XaPlayer *player, int track) {
    if (track < 0 || track >= PS1_XA_TRACK_COUNT) return;

    const Ps1XaTrackEntry *entry = &s_tracks[track];
    player->reader = NULL;
    player->reader_context = NULL;
    player->filter_file = -1;
    player->track = track;
    player->file = entry->file;
    player->channel = entry->channel;
    player->track_volume = entry->volume;
    player->fade_volume = (int16_t)entry->volume;
    player->serial_volume = 45;
    player->sector_index = 0;
    player->sector_sample_count = 0;
    player->sector_sample_idx = 0;
    player->is_stereo = 0;
    player->sample_rate = 18900;
    player->resample_step = (18900u * 65536u) / 44100u;
    player->hist1 = 0;
    player->hist2 = 0;
    player->hist1_l = 0;
    player->hist2_l = 0;
    player->hist1_r = 0;
    player->hist2_r = 0;
    player->frac_pos = 0;
    player->state = PS1_XA_PLAYING;

    prime_stream(player);
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

        int32_t samp_l = 0;
        int32_t samp_r = 0;

        if (player->is_stereo) {
            int32_t frac = (int32_t)(player->frac_pos & 0xFFFF);
            int32_t diff_l = (int32_t)player->next_sample - (int32_t)player->curr_sample;
            samp_l = (int32_t)player->curr_sample + (int32_t)(((int64_t)diff_l * frac) >> 16);
            int32_t diff_r = (int32_t)player->next_sample_r - (int32_t)player->curr_sample_r;
            samp_r = (int32_t)player->curr_sample_r + (int32_t)(((int64_t)diff_r * frac) >> 16);

            player->frac_pos += player->resample_step;
            while (player->frac_pos >= 0x10000 && player->state != PS1_XA_IDLE) {
                player->curr_sample = player->next_sample;
                player->curr_sample_r = player->next_sample_r;
                player->next_sample = fetch_xa_raw_sample(player);
                player->next_sample_r = fetch_xa_raw_sample(player);
                player->frac_pos -= 0x10000;
            }
        } else {
            /* Mono resampling */
            int32_t frac = (int32_t)(player->frac_pos & 0xFFFF);
            int32_t diff = (int32_t)player->next_sample - (int32_t)player->curr_sample;
            int32_t samp = (int32_t)player->curr_sample + (int32_t)(((int64_t)diff * frac) >> 16);
            samp_l = samp;
            samp_r = samp;

            player->frac_pos += player->resample_step;
            while (player->frac_pos >= 0x10000 && player->state != PS1_XA_IDLE) {
                player->curr_sample = player->next_sample;
                player->next_sample = fetch_xa_raw_sample(player);
                player->frac_pos -= 0x10000;
            }
        }

        /* Volume scaling: sample * serial_vol (out of 128) * cd_vol (out of 32767) */
        int32_t out_l = (int32_t)(((int64_t)samp_l * svol * cvol_l) / (128 * 32767));
        int32_t out_r = (int32_t)(((int64_t)samp_r * svol * cvol_r) / (128 * 32767));

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
