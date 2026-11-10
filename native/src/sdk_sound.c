#include "sdk_sound.h"
#include "sdk_cd.h"
#include <libsnd.h>
#include <libspu.h>
#include <stdlib.h>
#include <string.h>

#define SDK_MAX_VABS 8
#define SDK_MAX_SEQS 8
#define VAB_MAGIC 0x56414270u

typedef struct SdkVabRecord {
    bool head_open;
    bool body_loaded;
    uint8_t *header_buf;
    uint32_t header_size;
    uint8_t *body_buf;
    uint32_t body_capacity;
    uint32_t body_transferred;
} SdkVabRecord;

typedef struct SdkSeqRecord {
    bool open;
    bool playing;
    bool paused;
    int16_t vab_id;
    int16_t vol_left;
    int16_t vol_right;
    int16_t loop_count;
    const uint8_t *data;
    size_t data_size;
    Ps1Seq seq;
} SdkSeqRecord;

static Ps1Spu spu;
static SdkSerialVol serial_vol[2];
static uint8_t serial_attr[2];
static uint8_t serial_mode[2];
static SdkVabRecord vab_records[SDK_MAX_VABS];
static SdkSeqRecord seq_records[SDK_MAX_SEQS];
static const void *sequence_source;
static size_t sequence_source_size;
static bool sound_initialized;
static bool sound_running;
static bool sound_stereo_enabled = true;
static bool reverb_enabled;
static bool cd_mix_enabled;
static int32_t tick_mode_val;
static int32_t reserved_voices_val = PS1_SPU_VOICES;
static int32_t table_s_max = SDK_MAX_SEQS;
static int32_t table_t_max = 1;
static int16_t master_vol_l = 127;
static int16_t master_vol_r = 127;
static int16_t reverb_type_val;
static int16_t reverb_depth_l;
static int16_t reverb_depth_r;
static int16_t reverb_vol_l;
static int16_t reverb_vol_r;
static int16_t reverb_feedback_val;
static int16_t reverb_delay_val;

static void free_vab_record(int slot) {
    if (slot < 0 || slot >= SDK_MAX_VABS) return;
    for (int i = 0; i < SDK_MAX_SEQS; ++i) {
        if (seq_records[i].open && seq_records[i].vab_id == slot) {
            SsSeqStop(i);
        }
    }
    ps1_spu_free_vab(&spu, slot);
    free(vab_records[slot].header_buf);
    free(vab_records[slot].body_buf);
    memset(&vab_records[slot], 0, sizeof(vab_records[slot]));
}

static bool load_vab_into_spu_slot(int slot) {
    SdkVabRecord *rec = &vab_records[slot];
    Ps1VabSlot *v = &spu.vabs[slot];
    const uint8_t *header = rec->header_buf;
    uint32_t header_size = rec->header_size;

    memset(v, 0, sizeof(*v));
    v->header = header;
    v->header_size = header_size;
    v->body = rec->body_buf;
    v->body_size = rec->body_capacity;

    memcpy(&v->num_progs, header + 18, 2);
    memcpy(&v->num_tones, header + 20, 2);
    memcpy(&v->num_vags, header + 22, 2);
    if (v->num_progs > 128 || v->num_vags > 255) return false;

    uint32_t tone_count = (uint32_t)v->num_progs * 16u;
    uint32_t offtbl_pos = 32u + 2048u + tone_count * 32u;
    if (offtbl_pos + 512u > header_size) return false;

    unsigned packed_program = 0;
    for (unsigned program = 0; program < 128; ++program) {
        unsigned tones = header[32u + program * 16u];
        if (tones > 16) return false;
        v->program_tone_counts[program] = (uint8_t)tones;
        if (tones != 0) {
            if (packed_program >= v->num_progs) return false;
            v->program_tone_offsets[program] = 0x820u + packed_program++ * 512u;
        }
    }
    if (packed_program != v->num_progs) return false;

    const uint16_t *offtbl = (const uint16_t *)(const void *)(header + offtbl_pos);
    uint32_t cum_offset = (uint32_t)offtbl[0] * 8u;
    for (uint32_t i = 1; i <= v->num_vags && i < 256; ++i) {
        v->vag_body_offsets[i] = cum_offset;
        v->vag_lengths[i] = (uint32_t)offtbl[i] * 8u;
        if (cum_offset > v->body_size || v->vag_lengths[i] > v->body_size - cum_offset) {
            return false;
        }
        cum_offset += v->vag_lengths[i];
    }
    v->loaded = 1;
    rec->body_loaded = true;
    return true;
}

void sdk_sound_sequence_source(const void *data, size_t size) {
    sequence_source = data;
    sequence_source_size = size;
}

void sdk_sound_reset(void) {
    for (int i = 0; i < SDK_MAX_VABS; ++i) {
        free_vab_record(i);
    }
    memset(seq_records, 0, sizeof(seq_records));
    sequence_source = NULL;
    sequence_source_size = 0;
    ps1_spu_init(&spu);
    serial_vol[0] = (SdkSerialVol){0, 0};
    serial_vol[1] = (SdkSerialVol){0, 0};
    serial_attr[0] = serial_attr[1] = 0;
    serial_mode[0] = serial_mode[1] = 0;
    sound_initialized = false;
    sound_running = false;
    sound_stereo_enabled = true;
    reverb_enabled = false;
    cd_mix_enabled = false;
    tick_mode_val = 0;
    reserved_voices_val = PS1_SPU_VOICES;
    table_s_max = SDK_MAX_SEQS;
    table_t_max = 1;
    master_vol_l = 127;
    master_vol_r = 127;
    reverb_type_val = 0;
    reverb_depth_l = 0;
    reverb_depth_r = 0;
    reverb_vol_l = 0;
    reverb_vol_r = 0;
    reverb_feedback_val = 0;
    reverb_delay_val = 0;
}

SdkSerialVol sdk_sound_serial_vol(int s_num) {
    if (s_num < 0 || s_num > 1) return (SdkSerialVol){0, 0};
    return serial_vol[s_num];
}

SdkSoundStatus sdk_sound_status(void) {
    int32_t open_vabs = 0;
    int32_t open_seqs = 0;
    int32_t playing_seqs = 0;
    for (int i = 0; i < SDK_MAX_VABS; ++i) {
        if (vab_records[i].head_open) ++open_vabs;
    }
    for (int i = 0; i < SDK_MAX_SEQS; ++i) {
        if (seq_records[i].open) ++open_seqs;
        if (seq_records[i].open && seq_records[i].playing && !seq_records[i].paused) ++playing_seqs;
    }
    return (SdkSoundStatus){
        .initialized = sound_initialized,
        .running = sound_running,
        .stereo = sound_stereo_enabled,
        .reverb_enabled = reverb_enabled,
        .cd_mix = cd_mix_enabled,
        .tick_mode = tick_mode_val,
        .reserved_voices = reserved_voices_val,
        .master_vol_left = master_vol_l,
        .master_vol_right = master_vol_r,
        .reverb_type = reverb_type_val,
        .reverb_depth_left = reverb_depth_l,
        .reverb_depth_right = reverb_depth_r,
        .reverb_vol_left = reverb_vol_l,
        .reverb_vol_right = reverb_vol_r,
        .reverb_feedback = reverb_feedback_val,
        .reverb_delay = reverb_delay_val,
        .open_vab_count = open_vabs,
        .open_seq_count = open_seqs,
        .playing_seq_count = playing_seqs,
    };
}

const Ps1Spu *sdk_sound_spu(void) {
    return &spu;
}

void sdk_sound_mix(int16_t *out, int frames) {
    if (!out || frames <= 0) return;
    int16_t cd[256 * 2];
    spu.mono_voices = !sound_stereo_enabled;
    for (int first = 0; first < frames; first += 256) {
        int count = frames - first;
        if (count > 256) count = 256;
        sdk_cd_mix(cd, count); /* Drain even muted input on the same clock. */
        spu.cd_input = cd;
        spu.cd_input_frames = (unsigned)count;
        spu.cd_input_position = 0;
        spu.cd_mix = cd_mix_enabled;
        bool seq_mixed = false;
        for (int i = 0; i < SDK_MAX_SEQS; ++i) {
            SdkSeqRecord *slot = &seq_records[i];
            if (slot->open && slot->playing && !slot->paused && slot->seq.active) {
                ps1_seq_mix(&slot->seq, &spu, out + first * 2, count);
                seq_mixed = true;
                break;
            }
        }
        if (!seq_mixed) ps1_spu_mix(&spu, out + first * 2, count, 0);
        spu.cd_input = NULL; /* The borrowed stack buffer cannot escape. */
        spu.cd_input_frames = spu.cd_input_position = 0;
    }
}

void SsInit(void) {
    sdk_sound_reset();
    sound_initialized = true;
}

void SsSetTableSize(char *table, int s_max, int t_max) {
    if (!table || s_max <= 0 || s_max > SDK_MAX_SEQS || t_max <= 0) abort();
    table_s_max = s_max;
    table_t_max = t_max;
}

void SsSetTickMode(long tick_mode) {
    if (tick_mode <= 0) abort();
    tick_mode_val = (int32_t)tick_mode;
}

char SsSetReservedVoice(char voices) {
    if (voices < 1 || voices > PS1_SPU_VOICES) return -1;
    reserved_voices_val = voices;
    return voices;
}

void SsStart(void) {
    sound_running = true;
}

void SsEnd(void) {
    sound_running = false;
}

void SsSetMVol(int voll, int volr) {
    if (voll < 0) voll = 0;
    if (voll > 127) voll = 127;
    if (volr < 0) volr = 0;
    if (volr > 127) volr = 127;
    master_vol_l = (int16_t)voll;
    master_vol_r = (int16_t)volr;
    ps1_spu_set_master_vol(&spu,
                           (int16_t)((voll * 0x3FFF) / 127),
                           (int16_t)((volr * 0x3FFF) / 127));
}

void SsSetRVol(int voll, int volr) {
    if (voll < 0) voll = 0;
    if (voll > 127) voll = 127;
    if (volr < 0) volr = 0;
    if (volr > 127) volr = 127;
    reverb_vol_l = (int16_t)voll;
    reverb_vol_r = (int16_t)volr;
    reverb_depth_l = (int16_t)voll;
    reverb_depth_r = (int16_t)volr;
    ps1_spu_set_reverb_params(&spu, reverb_type_val, voll, volr,
        voll, volr, reverb_feedback_val, reverb_delay_val);
}

void SsSetStereo(void) {
    sound_stereo_enabled = true;
}

void SsSetMono(void) {
    sound_stereo_enabled = false;
}

void SsSetSerialAttr(char s_num, char attr, char mode) {
    if (s_num < 0 || s_num > 1 || attr < 0 || attr > 1 || mode < 0 || mode > 1) abort();
    serial_attr[(int)s_num] = (uint8_t)attr;
    serial_mode[(int)s_num] = (uint8_t)mode;
    if (s_num == SS_SERIAL_A && attr == SS_MIX) cd_mix_enabled = mode != 0;
    if (s_num == SS_SERIAL_A && attr == SS_REV) spu.cd_reverb = mode != 0;
}

void SsSetSerialVol(short s_num, short voll, short volr) {
    if (s_num < 0 || s_num > 1) abort();
    /* The original CD fade can request 128 (32 * 4). The reference SDK
     * saturates values above 127 instead of treating them as fatal errors. */
    if (voll > 127) voll = 127;
    if (volr > 127) volr = 127;
    serial_vol[s_num] = (SdkSerialVol){voll, volr};
    /* Keep the register's signed 16-bit conversion, including phase inversion. */
    if (s_num == SS_SERIAL_A)
        ps1_spu_set_cd_vol(&spu, (int16_t)(voll * 258), (int16_t)(volr * 258));
}

short SsVabOpenHead(u_char *addr, int vabid) {
    if (!addr) return -1;
    uint32_t magic = 0;
    memcpy(&magic, addr, 4);
    if (magic != VAB_MAGIC) return -1;

    uint16_t num_progs = 0;
    uint16_t num_vags = 0;
    memcpy(&num_progs, addr + 18, 2);
    memcpy(&num_vags, addr + 22, 2);
    if (num_progs > 128 || num_vags > 255) return -1;

    int slot = -1;
    if (vabid >= 0 && vabid < SDK_MAX_VABS) {
        if (vab_records[vabid].head_open) return -1;
        slot = vabid;
    } else {
        for (int i = 0; i < SDK_MAX_VABS; ++i) {
            if (!vab_records[i].head_open) {
                slot = i;
                break;
            }
        }
    }
    if (slot < 0) return -1;

    uint32_t offtbl_pos = 32u + 2048u + (uint32_t)num_progs * 16u * 32u;
    uint32_t header_size = offtbl_pos + 512u;
    const uint16_t *offtbl = (const uint16_t *)(const void *)(addr + offtbl_pos);
    uint32_t body_size = 0;
    for (uint32_t i = 0; i <= num_vags && i < 256; ++i) {
        body_size += (uint32_t)offtbl[i] * 8u;
    }

    uint8_t *header_copy = (uint8_t *)malloc(header_size);
    if (!header_copy) return -1;
    memcpy(header_copy, addr, header_size);

    uint8_t *body_copy = NULL;
    if (body_size > 0) {
        body_copy = (uint8_t *)calloc(1, body_size);
        if (!body_copy) {
            free(header_copy);
            return -1;
        }
    }

    SdkVabRecord *rec = &vab_records[slot];
    rec->head_open = true;
    rec->body_loaded = false;
    rec->header_buf = header_copy;
    rec->header_size = header_size;
    rec->body_buf = body_copy;
    rec->body_capacity = body_size;
    rec->body_transferred = 0;

    if (!load_vab_into_spu_slot(slot)) {
        free_vab_record(slot);
        return -1;
    }
    return (short)slot;
}

short SsVabTransBodyPartly(u_char *addr, u_long msize, int vabid) {
    if (!addr || vabid < 0 || vabid >= SDK_MAX_VABS || !vab_records[vabid].head_open) {
        return -1;
    }
    SdkVabRecord *rec = &vab_records[vabid];
    if (rec->body_transferred < rec->body_capacity) {
        uint32_t remaining = rec->body_capacity - rec->body_transferred;
        uint32_t copy_bytes = msize < remaining ? (uint32_t)msize : remaining;
        memcpy(rec->body_buf + rec->body_transferred, addr, copy_bytes);
        rec->body_transferred += copy_bytes;
    }
    return (short)vabid;
}

short SsVabTransCompleted(short immediateFlag) {
    (void)immediateFlag;
    return 1;
}

void SsVabClose(int vabid) {
    if (vabid < 0 || vabid >= SDK_MAX_VABS) return;
    free_vab_record(vabid);
}

short SsSeqOpen(u_long *addr, int vab_id) {
    const uint8_t *bytes = sequence_source;
    size_t size = sequence_source_size;
    sequence_source = NULL;
    sequence_source_size = 0;
    if (!addr || (const void *)addr != bytes || size < 16) return -1;
    Ps1Seq sequence;
    if (!ps1_seq_open(&sequence, bytes, size, vab_id, true)) return -1;
    int slot = -1;
    for (int i = 0; i < table_s_max && i < SDK_MAX_SEQS; ++i) {
        if (!seq_records[i].open) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    SdkSeqRecord *rec = &seq_records[slot];
    memset(rec, 0, sizeof(*rec));
    rec->open = true;
    rec->vab_id = (int16_t)vab_id;
    rec->vol_left = 127;
    rec->vol_right = 127;
    rec->data = bytes;
    rec->data_size = size;
    rec->seq = sequence;
    return (short)slot;
}

void SsSeqPlay(int seq_access_num, char play_mode, short l_count) {
    if (seq_access_num < 0 || seq_access_num >= SDK_MAX_SEQS) return;
    SdkSeqRecord *rec = &seq_records[seq_access_num];
    if (!rec->open) return;
    rec->loop_count = l_count;
    if (play_mode == SSPLAY_PAUSE) {
        rec->paused = true;
        return;
    }
    ps1_seq_open(&rec->seq, rec->data, rec->data_size, rec->vab_id, l_count == SSPLAY_INFINITY);
    ps1_seq_set_vol(&rec->seq, &spu, (unsigned)rec->vol_left, (unsigned)rec->vol_right);
    rec->playing = true;
    rec->paused = false;
}

void SsSeqPause(int seq_access_num) {
    if (seq_access_num < 0 || seq_access_num >= SDK_MAX_SEQS) return;
    SdkSeqRecord *rec = &seq_records[seq_access_num];
    if (rec->open && rec->playing) {
        rec->paused = true;
    }
}

void SsSeqReplay(int seq_access_num) {
    if (seq_access_num < 0 || seq_access_num >= SDK_MAX_SEQS) return;
    SdkSeqRecord *rec = &seq_records[seq_access_num];
    if (rec->open && rec->paused) {
        rec->paused = false;
        rec->playing = true;
    }
}

void SsSeqStop(int seq_access_num) {
    if (seq_access_num < 0 || seq_access_num >= SDK_MAX_SEQS) return;
    SdkSeqRecord *rec = &seq_records[seq_access_num];
    if (!rec->open) return;
    rec->playing = false;
    rec->paused = false;
    rec->seq.active = false;
    for (int v = 0; v < PS1_SPU_VOICES; ++v) {
        if (spu.voices[v].vab_id == rec->vab_id) {
            ps1_spu_key_off(&spu, v);
        }
    }
}

void SsSeqSetVol(int seq_access_num, int voll, int volr) {
    if (seq_access_num < 0 || seq_access_num >= SDK_MAX_SEQS) return;
    SdkSeqRecord *rec = &seq_records[seq_access_num];
    if (!rec->open) return;
    if (voll < 0) voll = 0;
    if (voll > 127) voll = 127;
    if (volr < 0) volr = 0;
    if (volr > 127) volr = 127;
    rec->vol_left = (int16_t)voll;
    rec->vol_right = (int16_t)volr;
    ps1_seq_set_vol(&rec->seq, &spu, (unsigned)voll, (unsigned)volr);
}

void SsSeqClose(int seq_access_num) {
    if (seq_access_num < 0 || seq_access_num >= SDK_MAX_SEQS) return;
    SsSeqStop(seq_access_num);
    memset(&seq_records[seq_access_num], 0, sizeof(seq_records[seq_access_num]));
}

short SsUtKeyOnV(int voice, int vabId, int prog, int tone, int note, int fine, int voll, int volr) {
    if (voice < 0 || voice >= PS1_SPU_VOICES) return -1;
    ps1_spu_key_on_vab(&spu, voice, vabId, prog, tone, note, fine, voll, volr);
    return (short)voice;
}

short SsUtKeyOn(int vabId, int prog, int tone, int note, int fine, int voll, int volr) {
    int limit = reserved_voices_val > 0 && reserved_voices_val <= PS1_SPU_VOICES
                    ? reserved_voices_val
                    : PS1_SPU_VOICES;
    int chosen = -1;
    for (int v = 0; v < limit; ++v) {
        if (!ps1_spu_voice_is_active(&spu, v)) {
            chosen = v;
            break;
        }
    }
    if (chosen < 0) return -1;
    return SsUtKeyOnV(chosen, vabId, prog, tone, note, fine, voll, volr);
}

short SsUtKeyOffV(int voice) {
    if (voice < 0 || voice >= PS1_SPU_VOICES) return -1;
    ps1_spu_key_off(&spu, voice);
    return 0;
}

short SsUtKeyOff(int voice, int vabId, int prog, int tone, int note) {
    (void)vabId;
    (void)prog;
    (void)tone;
    (void)note;
    return SsUtKeyOffV(voice);
}

void SsUtAllKeyOff(int mode) {
    (void)mode;
    ps1_spu_key_off_all(&spu);
}

short SsUtSetVVol(int voice, int voll, int volr) {
    if (voice < 0 || voice >= PS1_SPU_VOICES) return -1;
    if (voll < 0) voll = 0;
    if (voll > 127) voll = 127;
    if (volr < 0) volr = 0;
    if (volr > 127) volr = 127;
    spu.voices[voice].vol_left = (int16_t)((voll * 0x7FFF) / 127);
    spu.voices[voice].vol_right = (int16_t)((volr * 0x7FFF) / 127);
    return 0;
}

void SsVoKeyOff(long vab_pro, long pitch) {
    (void)pitch;
    int voice = (int)(vab_pro & 0xFF);
    if (voice >= 0 && voice < PS1_SPU_VOICES) {
        ps1_spu_key_off(&spu, voice);
    }
}

void SsUtReverbOn(void) {
    reverb_enabled = true;
    ps1_spu_set_reverb_enabled(&spu, 1);
}

void SsUtReverbOff(void) {
    reverb_enabled = false;
    ps1_spu_set_reverb_enabled(&spu, 0);
}

short SsUtSetReverbType(short type) {
    if (type < -9 || type > 9) return -1;
    int mode = type < 0 ? -type : type;
    if (mode != reverb_type_val) {
        reverb_delay_val = mode == 7 || mode == 8 ? 127 : 0;
        reverb_feedback_val = mode == 7 ? 127 : 0;
    }
    reverb_type_val = (int16_t)mode;
    if (!mode) {
        reverb_enabled = false;
        ps1_spu_set_reverb_enabled(&spu, 0);
    }
    ps1_spu_set_reverb_params(&spu, type, reverb_depth_l, reverb_depth_r,
        reverb_vol_l, reverb_vol_r, reverb_feedback_val, reverb_delay_val);
    return (short)mode;
}

void SsUtSetReverbDepth(short ldepth, short rdepth) {
    reverb_depth_l = ldepth;
    reverb_depth_r = rdepth;
    reverb_vol_l = ldepth;
    reverb_vol_r = rdepth;
    ps1_spu_set_reverb_params(&spu, reverb_type_val, reverb_depth_l, reverb_depth_r,
                              reverb_vol_l, reverb_vol_r, reverb_feedback_val, reverb_delay_val);
}

void SsUtSetReverbFeedback(short feedback) {
    reverb_feedback_val = feedback;
    ps1_spu_set_reverb_params(&spu, reverb_type_val, reverb_depth_l, reverb_depth_r,
                              reverb_vol_l, reverb_vol_r, reverb_feedback_val, reverb_delay_val);
}

void SsUtSetReverbDelay(short delay) {
    reverb_delay_val = delay;
    ps1_spu_set_reverb_params(&spu, reverb_type_val, reverb_depth_l, reverb_depth_r,
                              reverb_vol_l, reverb_vol_r, reverb_feedback_val, reverb_delay_val);
}

void SpuSetCommonAttr(SpuCommonAttr *attr) {
    if (!attr) return;
    unsigned long mask = attr->mask;
    int16_t mvol_l = spu.master_vol_left;
    int16_t mvol_r = spu.master_vol_right;
    int16_t cd_l = spu.cd_vol_left;
    int16_t cd_r = spu.cd_vol_right;

    if (mask == 0 || (mask & SPU_COMMON_MVOLL)) mvol_l = attr->mvol.left;
    if (mask == 0 || (mask & SPU_COMMON_MVOLR)) mvol_r = attr->mvol.right;
    if (mask == 0 || (mask & SPU_COMMON_CDVOLL)) cd_l = attr->cd.volume.left;
    if (mask == 0 || (mask & SPU_COMMON_CDVOLR)) cd_r = attr->cd.volume.right;
    if (mask == 0 || (mask & SPU_COMMON_CDMIX)) cd_mix_enabled = (attr->cd.mix != 0);
    if (mask == 0 || (mask & SPU_COMMON_CDREV)) spu.cd_reverb = attr->cd.reverb != 0;

    ps1_spu_set_master_vol(&spu, mvol_l, mvol_r);
    ps1_spu_set_cd_vol(&spu, cd_l, cd_r);
}

void SpuGetAllKeysStatus(void *status) {
    if (!status) return;
    uint8_t *bytes = (uint8_t *)status;
    for (int v = 0; v < PS1_SPU_VOICES; ++v) {
        bytes[v] = (uint8_t)(ps1_spu_voice_is_active(&spu, v) ? SPU_ON : SPU_OFF);
    }
}

long SpuGetKeyStatus(unsigned long voice_bit) {
    for (int v = 0; v < PS1_SPU_VOICES; ++v) {
        if ((voice_bit & (1UL << v)) && ps1_spu_voice_is_active(&spu, v)) {
            return SPU_ON;
        }
    }
    return SPU_OFF;
}

unsigned long SpuSetReverbVoice(long on, unsigned long voice_bit) {
    return ps1_spu_set_reverb_voice(&spu, (uint32_t)voice_bit, on != 0);
}
