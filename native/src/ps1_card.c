/* ps1_card.c — host-side virtual PS1 memory card system. */

#include "ps1_card.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static const char *s_farm_filenames[PS1_CARD_SAVE_SLOTS] = {
    "BASLUS-01115-FARM0",
    "BASLUS-01115-FARM1",
    "BASLUS-01115-FARM2"
};

const char *ps1_card_get_farm_filename(int slot_idx) {
    if (slot_idx < 0 || slot_idx >= PS1_CARD_SAVE_SLOTS) return NULL;
    return s_farm_filenames[slot_idx];
}

uint8_t ps1_card_checksum(const uint8_t *frame128) {
    uint8_t c = 0;
    for (size_t i = 0; i < 127; i++) {
        c ^= frame128[i];
    }
    return c;
}

void ps1_card_init(Ps1Card *card) {
    if (!card) return;
    memset(card, 0, sizeof(*card));
    card->present = false;
    card->dirty = false;
}

void ps1_card_format(Ps1Card *card) {
    if (!card) return;
    memset(card->data, 0, PS1_CARD_SIZE);

    /* Frame 0: Header frame */
    card->data[0] = 'M';
    card->data[1] = 'C';
    card->data[127] = ps1_card_checksum(card->data);

    /* Frames 1..15: Directory entries for blocks 1..15 */
    for (uint32_t b = 1; b <= PS1_CARD_DATA_BLOCKS; b++) {
        uint8_t *dir = card->data + b * PS1_CARD_FRAME_SIZE;
        dir[0] = PS1_CARD_BLOCK_FREE; /* 0xA0 */
        dir[8] = 0xFF; /* Next block = 0xFFFF (end) */
        dir[9] = 0xFF;
        dir[127] = ps1_card_checksum(dir);
    }

    /* No broken sectors: zero would incorrectly designate sector zero.
     * Frame 63 is the format/write-test copy of the card header. */
    for (unsigned frame = 16; frame < 36; ++frame) {
        uint8_t *entry = card->data + frame * PS1_CARD_FRAME_SIZE;
        memset(entry, 0xFF, 4);
        entry[8] = entry[9] = 0xFF;
        entry[127] = ps1_card_checksum(entry);
    }
    memcpy(card->data + 63 * PS1_CARD_FRAME_SIZE, card->data, PS1_CARD_FRAME_SIZE);
    card->present = true;
    card->dirty = true;
}

static bool free_directory(uint8_t tag) {
    return tag >= 0xA0 && tag <= 0xA3; /* Fresh or deleted blocks. */
}

/* File links on disk are 0..14; all in-memory block addresses are 1..15.
 * Validate an entire chain before a caller can modify data or directories.
 * Format reference: psx-spx, Controllers and Memory Cards / Data Format. */
static bool walk_file_blocks(const Ps1Card *card, unsigned block, uint32_t size,
    uint8_t *blocks, unsigned *used) {
    if (!size || size > PS1_CARD_DATA_BLOCKS * PS1_CARD_BLOCK_SIZE ||
        size % PS1_CARD_BLOCK_SIZE != 0) return false;
    unsigned count = (size + PS1_CARD_BLOCK_SIZE - 1) / PS1_CARD_BLOCK_SIZE;
    unsigned seen = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (block < 1 || block > PS1_CARD_DATA_BLOCKS || (seen & (1u << block)))
            return false;
        const uint8_t *dir = card->data + block * PS1_CARD_FRAME_SIZE;
        unsigned tag = i == 0 ? PS1_CARD_BLOCK_FIRST :
            i + 1 == count ? PS1_CARD_BLOCK_LAST : PS1_CARD_BLOCK_MIDDLE;
        unsigned next = dir[8] | ((unsigned)dir[9] << 8);
        if (dir[0] != tag || dir[1] || dir[2] || dir[3]) return false;
        if (i + 1 == count ? next != 0xFFFF : next >= PS1_CARD_DATA_BLOCKS)
            return false;
        if (blocks) blocks[i] = (uint8_t)block;
        seen |= 1u << block;
        block = next + 1;
    }
    if (used) *used = seen;
    return true;
}

Ps1CardStatus ps1_card_validate(const Ps1Card *card) {
    if (!card) return PS1_CARD_ERR_NO_CARD;

    /* Frame 0 magic check */
    if (card->data[0] != 'M' || card->data[1] != 'C') {
        return PS1_CARD_ERR_UNFORMATTED;
    }

    /* Frame 0 checksum */
    if (card->data[127] != ps1_card_checksum(card->data)) {
        return PS1_CARD_ERR_DAMAGED;
    }

    /* Validate directory frames 1..15 */
    for (uint32_t b = 1; b <= PS1_CARD_DATA_BLOCKS; b++) {
        const uint8_t *dir = card->data + b * PS1_CARD_FRAME_SIZE;
        if (dir[127] != ps1_card_checksum(dir)) {
            return PS1_CARD_ERR_DAMAGED;
        }
    }

    /* Reject shared tails, orphaned allocated blocks, and unknown states.
     * A checksum alone cannot make an unsafe allocation graph valid. */
    unsigned allocated = 0, owned = 0;
    for (unsigned block = 1; block <= PS1_CARD_DATA_BLOCKS; ++block) {
        const uint8_t *dir = card->data + block * PS1_CARD_FRAME_SIZE;
        if (dir[1] || dir[2] || dir[3]) return PS1_CARD_ERR_DAMAGED;
        if (free_directory(dir[0])) continue;
        if (dir[0] < PS1_CARD_BLOCK_FIRST || dir[0] > PS1_CARD_BLOCK_LAST)
            return PS1_CARD_ERR_DAMAGED;
        allocated |= 1u << block;
        if (dir[0] == PS1_CARD_BLOCK_FIRST) {
            uint32_t size;
            unsigned used;
            memcpy(&size, dir + 4, sizeof(size));
            if (!walk_file_blocks(card, block, size, NULL, &used) || (owned & used))
                return PS1_CARD_ERR_DAMAGED;
            owned |= used;
        }
    }
    return owned == allocated ? PS1_CARD_OK : PS1_CARD_ERR_DAMAGED;
}

int ps1_card_get_free_blocks(const Ps1Card *card) {
    if (!card || ps1_card_validate(card) != PS1_CARD_OK) return 0;

    int free_blocks = 0;
    for (uint32_t b = 1; b <= PS1_CARD_DATA_BLOCKS; b++) {
        const uint8_t *dir = card->data + b * PS1_CARD_FRAME_SIZE;
        if (free_directory(dir[0])) {
            free_blocks++;
        }
    }
    return free_blocks;
}

int ps1_card_find_file(const Ps1Card *card, const char *filename, uint32_t *out_size) {
    if (!card || !filename || ps1_card_validate(card) != PS1_CARD_OK) return 0;

    for (uint32_t b = 1; b <= PS1_CARD_DATA_BLOCKS; b++) {
        const uint8_t *dir = card->data + b * PS1_CARD_FRAME_SIZE;
        if (dir[0] == PS1_CARD_BLOCK_FIRST) {
            char name_buf[21];
            memcpy(name_buf, dir + 10, 20);
            name_buf[20] = '\0';
            if (strcmp(name_buf, filename) == 0) {
                if (out_size) {
                    uint32_t sz;
                    memcpy(&sz, dir + 4, 4);
                    *out_size = sz;
                }
                return (int)b;
            }
        }
    }
    return 0;
}

Ps1CardStatus ps1_card_file_blocks(const Ps1Card *card, const char *filename,
    uint8_t blocks[PS1_CARD_DATA_BLOCKS], uint32_t *out_size) {
    if (!filename || !blocks) return PS1_CARD_ERR_IO;
    Ps1CardStatus status = ps1_card_validate(card);
    if (status != PS1_CARD_OK) return status;
    uint32_t size;
    int start = ps1_card_find_file(card, filename, &size);
    if (!start) return PS1_CARD_ERR_NO_FILE;
    uint8_t chain[PS1_CARD_DATA_BLOCKS];
    if (!walk_file_blocks(card, (unsigned)start, size, chain, NULL))
        return PS1_CARD_ERR_DAMAGED;
    unsigned count = (size + PS1_CARD_BLOCK_SIZE - 1) / PS1_CARD_BLOCK_SIZE;
    memcpy(blocks, chain, count);
    if (out_size) *out_size = size;
    return PS1_CARD_OK;
}

Ps1CardStatus ps1_card_read_file(const Ps1Card *card, const char *filename,
    void *out_buf, size_t max_size, size_t *out_size) {
    if (!out_buf) return PS1_CARD_ERR_IO;
    uint8_t chain[PS1_CARD_DATA_BLOCKS];
    uint32_t size;
    Ps1CardStatus status = ps1_card_file_blocks(card, filename, chain, &size);
    if (status != PS1_CARD_OK) return status;
    size_t length = size < max_size ? size : max_size;
    size_t done = 0;
    for (unsigned i = 0; done < length; ++i) {
        size_t chunk = length - done;
        if (chunk > PS1_CARD_BLOCK_SIZE) chunk = PS1_CARD_BLOCK_SIZE;
        memcpy((uint8_t *)out_buf + done,
            card->data + chain[i] * PS1_CARD_BLOCK_SIZE, chunk);
        done += chunk;
    }
    if (out_size) *out_size = length;
    return PS1_CARD_OK;
}

Ps1CardStatus ps1_card_delete_file(Ps1Card *card, const char *filename) {
    uint8_t chain[PS1_CARD_DATA_BLOCKS];
    uint32_t size;
    Ps1CardStatus status = ps1_card_file_blocks(card, filename, chain, &size);
    if (status != PS1_CARD_OK) return status;
    unsigned count = (size + PS1_CARD_BLOCK_SIZE - 1) / PS1_CARD_BLOCK_SIZE;
    for (unsigned i = 0; i < count; ++i) {
        uint8_t *dir = card->data + chain[i] * PS1_CARD_FRAME_SIZE;
        memset(dir, 0, PS1_CARD_FRAME_SIZE);
        dir[0] = PS1_CARD_BLOCK_FREE;
        dir[8] = dir[9] = 0xFF;
        dir[127] = ps1_card_checksum(dir);
    }
    card->dirty = true;
    return PS1_CARD_OK;
}

static Ps1CardStatus write_file_in_place(Ps1Card *card, const char *filename,
                                  const void *data, size_t size) {
    if (!card || !filename || !data || size == 0) return PS1_CARD_ERR_IO;
    if (size > (size_t)PS1_CARD_DATA_BLOCKS * PS1_CARD_BLOCK_SIZE) return PS1_CARD_ERR_FULL;
    Ps1CardStatus validation = ps1_card_validate(card);
    if (validation != PS1_CARD_OK) return validation;

    uint32_t needed_blocks = (uint32_t)((size + PS1_CARD_BLOCK_SIZE - 1) / PS1_CARD_BLOCK_SIZE);
    if (needed_blocks == 0 || needed_blocks > PS1_CARD_DATA_BLOCKS) return PS1_CARD_ERR_FULL;

    /* If file exists, delete it first to free its blocks */
    uint32_t existing_sz = 0;
    if (ps1_card_find_file(card, filename, &existing_sz) > 0) {
        ps1_card_delete_file(card, filename);
    }

    if ((uint32_t)ps1_card_get_free_blocks(card) < needed_blocks) {
        return PS1_CARD_ERR_FULL;
    }

    /* Find free blocks */
    int allocated_blocks[PS1_CARD_DATA_BLOCKS];
    uint32_t found = 0;
    for (uint32_t b = 1; b <= PS1_CARD_DATA_BLOCKS && found < needed_blocks; b++) {
        const uint8_t *dir = card->data + b * PS1_CARD_FRAME_SIZE;
        if (free_directory(dir[0])) {
            allocated_blocks[found++] = (int)b;
        }
    }
    if (found < needed_blocks) return PS1_CARD_ERR_FULL;

    /* Write data into blocks and configure directory chain */
    const uint8_t *src = (const uint8_t *)data;
    size_t bytes_left = size;

    for (uint32_t i = 0; i < needed_blocks; i++) {
        int b = allocated_blocks[i];
        size_t chunk = (bytes_left > PS1_CARD_BLOCK_SIZE) ? PS1_CARD_BLOCK_SIZE : bytes_left;

        uint8_t *dst = card->data + (size_t)b * PS1_CARD_BLOCK_SIZE;
        memset(dst, 0, PS1_CARD_BLOCK_SIZE);
        memcpy(dst, src, chunk);

        src += chunk;
        bytes_left -= chunk;

        /* Update directory frame */
        uint8_t *dir = card->data + b * PS1_CARD_FRAME_SIZE;
        memset(dir, 0, PS1_CARD_FRAME_SIZE);

        if (i == 0) {
            dir[0] = PS1_CARD_BLOCK_FIRST;
            uint32_t sz32 = needed_blocks * PS1_CARD_BLOCK_SIZE;
            memcpy(dir + 4, &sz32, 4);
            size_t fn_len = strlen(filename);
            if (fn_len > 20) fn_len = 20;
            memcpy(dir + 10, filename, fn_len);
        } else if (i + 1 == needed_blocks) {
            dir[0] = PS1_CARD_BLOCK_LAST;
        } else {
            dir[0] = PS1_CARD_BLOCK_MIDDLE;
        }

        uint16_t next = (i + 1 < needed_blocks) ? (uint16_t)(allocated_blocks[i + 1] - 1) : 0xFFFF;
        memcpy(dir + 8, &next, 2);
        dir[127] = ps1_card_checksum(dir);
    }

    card->dirty = true;
    return PS1_CARD_OK;
}

Ps1CardStatus ps1_card_write_file(Ps1Card *card, const char *filename,
                                  const void *data, size_t size) {
    Ps1Card *staged;
    Ps1CardStatus status;
    if (!card || !filename || !data || size == 0 || strlen(filename) > 20) {
        return PS1_CARD_ERR_IO;
    }
    /* Check before rounding so an oversized size cannot wrap or truncate. */
    if (size > PS1_CARD_DATA_BLOCKS * PS1_CARD_BLOCK_SIZE) return PS1_CARD_ERR_FULL;
    staged = malloc(sizeof(*staged));
    if (!staged) return PS1_CARD_ERR_IO;
    memcpy(staged, card, sizeof(*staged));
    status = write_file_in_place(staged, filename, data, size);
    /* A failed replacement must leave the original save and directory intact. */
    if (status == PS1_CARD_OK) memcpy(card, staged, sizeof(*card));
    free(staged);
    return status;
}

/* -------------------------------------------------------------------------
 * Farm Slot Helpers
 * ---------------------------------------------------------------------- */
Ps1CardStatus ps1_card_read_farm_slot(const Ps1Card *card, int slot_idx,
                                      void *out_buf, size_t max_size, size_t *out_size) {
    const char *fn = ps1_card_get_farm_filename(slot_idx);
    if (!fn) return PS1_CARD_ERR_IO;
    return ps1_card_read_file(card, fn, out_buf, max_size, out_size);
}

Ps1CardStatus ps1_card_write_farm_slot(Ps1Card *card, int slot_idx,
                                       const void *data, size_t size) {
    const char *fn = ps1_card_get_farm_filename(slot_idx);
    if (!fn) return PS1_CARD_ERR_IO;
    return ps1_card_write_file(card, fn, data, size);
}

Ps1CardStatus ps1_card_read_slot_header(const Ps1Card *card, int slot_idx,
                                        void *out_header, size_t header_size) {
    if (!out_header || header_size == 0) return PS1_CARD_ERR_IO;

    uint8_t full_file[PS1_CARD_SAVE_SIZE];
    size_t actual_size = 0;
    Ps1CardStatus status = ps1_card_read_farm_slot(card, slot_idx, full_file, sizeof(full_file), &actual_size);
    if (status != PS1_CARD_OK) return status;

    if (actual_size < PS1_CARD_HEADER_OFFSET + PS1_CARD_HEADER_SIZE) {
        return PS1_CARD_ERR_DAMAGED;
    }

    const uint8_t *hdr = full_file + PS1_CARD_HEADER_OFFSET;

    /* Verify 1-byte XOR checksum across bytes 0..126 against byte 127 */
    uint8_t c = 0;
    for (size_t i = 0; i < 127; i++) {
        c ^= hdr[i];
    }
    if (c != hdr[127]) {
        return PS1_CARD_ERR_BAD_CHECKSUM;
    }

    size_t copy_len = (header_size > 127) ? 127 : header_size;
    memcpy(out_header, hdr, copy_len);
    return PS1_CARD_OK;
}

/* -------------------------------------------------------------------------
 * Persistence, Atomic Save, and Crash Recovery
 * ---------------------------------------------------------------------- */
/* A raw image must contain exactly one card, not just a plausible header. */
static bool read_card_image(FILE *file, Ps1Card *card) {
    size_t count = fread(card->data, 1, PS1_CARD_SIZE, file);
    int extra = fgetc(file);
    return count == PS1_CARD_SIZE && extra == EOF && !ferror(file);
}

static bool is_valid_mcr_file(const char *filepath) {
    if (!filepath) return false;
    FILE *file = fopen(filepath, "rb");
    if (!file) return false;
    Ps1Card *card = malloc(sizeof(*card));
    bool valid = card && read_card_image(file, card) &&
        ps1_card_validate(card) == PS1_CARD_OK;
    if (fclose(file) != 0) valid = false;
    free(card);
    return valid;
}

bool ps1_card_recover_file(const char *filepath) {
    if (!filepath) return false;

    char tmp_path[540];
    char bak_path[540];
    int len_tmp = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", filepath);
    int len_bak = snprintf(bak_path, sizeof(bak_path), "%s.bak", filepath);
    if (len_tmp < 0 || (size_t)len_tmp >= sizeof(tmp_path) ||
        len_bak < 0 || (size_t)len_bak >= sizeof(bak_path)) return false;

    /* If primary file is already valid, nothing to recover */
    if (is_valid_mcr_file(filepath)) {
        SDL_RemovePath(tmp_path);
        return true;
    }

    /* Replace directly: deleting the primary first loses it if rename fails. */
    if (is_valid_mcr_file(tmp_path)) {
        if (SDL_RenamePath(tmp_path, filepath)) return true;
    }

    /* Try recovering from .bak */
    if (is_valid_mcr_file(bak_path)) {
        if (SDL_RenamePath(bak_path, filepath)) return true;
    }

    return false;
}

bool ps1_card_load_from_file(Ps1Card *card, const char *filepath) {
    if (!card || !filepath) return false;

    ps1_card_init(card);
    int length = snprintf(card->filepath, sizeof(card->filepath), "%s", filepath);
    if (length < 0 || (size_t)length >= sizeof(card->filepath)) return false;

    ps1_card_recover_file(filepath);
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        /* An access error is not an empty slot. Create only a genuinely missing
         * image, and expose no usable card if its initial persistence fails. */
        if (errno != ENOENT) return false;
        char candidate[540];
        SDL_PathInfo info;
        snprintf(candidate, sizeof(candidate), "%s.tmp", filepath);
        if (SDL_GetPathInfo(candidate, &info)) return false;
        snprintf(candidate, sizeof(candidate), "%s.bak", filepath);
        if (SDL_GetPathInfo(candidate, &info)) return false;
        ps1_card_format(card);
        bool saved = ps1_card_save_to_file(card, filepath);
        card->present = saved;
        card->dirty = false;
        return saved;
    }

    bool complete = read_card_image(file, card);
    if (fclose(file) != 0) complete = false;
    if (!complete) return false;

    /* Preserve full-size invalid/unformatted media for the SDK error path.
     * Only an explicit format request may replace it; shutdown must not. */
    card->present = true;
    card->dirty = false;
    return ps1_card_validate(card) == PS1_CARD_OK;
}

bool ps1_card_rotate_backups(const char *filepath, int max_backups) {
    if (!filepath || max_backups <= 0) return false;
    char src[560], dst[560];
    for (int i = max_backups; i >= 1; --i) {
        if (i == 1) {
            snprintf(src, sizeof(src), "%s.bak", filepath);
            snprintf(dst, sizeof(dst), "%s.bak.1", filepath);
        } else {
            snprintf(src, sizeof(src), "%s.bak.%d", filepath, i - 1);
            snprintf(dst, sizeof(dst), "%s.bak.%d", filepath, i);
        }
        SDL_PathInfo info;
        if (SDL_GetPathInfo(src, &info)) {
            SDL_RemovePath(dst);
            SDL_RenamePath(src, dst);
        }
    }
    return true;
}

bool ps1_card_save_to_file(const Ps1Card *card, const char *filepath) {
    if (!card || !filepath || !card->present ||
        ps1_card_validate(card) != PS1_CARD_OK) return false;

    char tmp_path[540];
    char bak_path[540];
    char bak_hold[540];
    int len_tmp = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", filepath);
    int len_bak = snprintf(bak_path, sizeof(bak_path), "%s.bak", filepath);
    int len_hold = snprintf(bak_hold, sizeof(bak_hold), "%s.bak.hold", filepath);
    if (len_tmp < 0 || (size_t)len_tmp >= sizeof(tmp_path) ||
        len_bak < 0 || (size_t)len_bak >= sizeof(bak_path) ||
        len_hold < 0 || (size_t)len_hold >= sizeof(bak_hold)) return false;

    /* Never overwrite an unresolved recovery candidate from an earlier run. */
    FILE *f = fopen(tmp_path, "wbx");
    if (!f) return false;

    size_t written = fwrite(card->data, 1, PS1_CARD_SIZE, f);
    bool complete = written == PS1_CARD_SIZE;
    if (fflush(f) != 0) complete = false;
    if (fclose(f) != 0) complete = false;

    if (!complete || !is_valid_mcr_file(tmp_path)) {
        SDL_RemovePath(tmp_path);
        return false;
    }

    /* Move existing primary to hold location without touching .bak yet */
    SDL_PathInfo info;
    bool had_primary = false;
    if (SDL_GetPathInfo(filepath, &info)) {
        if (!SDL_RenamePath(filepath, bak_hold)) {
            SDL_RemovePath(tmp_path);
            return false;
        }
        had_primary = true;
    }

    /* Atomically replace target with temp file */
    if (!SDL_RenamePath(tmp_path, filepath)) {
        if (had_primary) {
            SDL_RenamePath(bak_hold, filepath);
        }
        SDL_RemovePath(tmp_path);
        return false;
    }

    /* Once primary is successfully replaced, rotate historical backups and promote hold to .bak */
    if (had_primary) {
        ps1_card_rotate_backups(filepath, 5);
        SDL_RemovePath(bak_path);
        SDL_RenamePath(bak_hold, bak_path);
    }

    return true;
}

/* -------------------------------------------------------------------------
 * Card Manager
 * ---------------------------------------------------------------------- */
bool ps1_card_manager_init(Ps1CardManager *mgr, const char *custom_dir) {
    if (!mgr) return false;
    memset(mgr, 0, sizeof(*mgr));

    if (custom_dir && custom_dir[0] != '\0') {
        snprintf(mgr->base_dir, sizeof(mgr->base_dir), "%s", custom_dir);
    } else {
        char *pref = SDL_GetPrefPath("VanillaBTN", "VanillaBTN");
        if (pref) {
            snprintf(mgr->base_dir, sizeof(mgr->base_dir), "%s", pref);
            SDL_free(pref);
        } else {
            snprintf(mgr->base_dir, sizeof(mgr->base_dir), ".");
        }
    }

    size_t blen = strlen(mgr->base_dir);
    while (blen > 1 && (mgr->base_dir[blen - 1] == '/' || mgr->base_dir[blen - 1] == '\\')) {
        if (blen == 3 && mgr->base_dir[1] == ':') break;
        mgr->base_dir[--blen] = '\0';
    }

    char card1_path[540];
    char card2_path[540];
    int len1 = snprintf(card1_path, sizeof(card1_path), "%s/card1.mcr", mgr->base_dir);
    int len2 = snprintf(card2_path, sizeof(card2_path), "%s/card2.mcr", mgr->base_dir);
    if (len1 < 0 || (size_t)len1 >= sizeof(card1_path) ||
        len2 < 0 || (size_t)len2 >= sizeof(card2_path)) return false;

    ps1_card_load_from_file(&mgr->cards[0], card1_path);
    ps1_card_load_from_file(&mgr->cards[1], card2_path);

    return true;
}

void ps1_card_manager_shutdown(Ps1CardManager *mgr) {
    if (!mgr) return;

    for (int i = 0; i < 2; i++) {
        if (mgr->cards[i].present && mgr->cards[i].dirty) {
            if (ps1_card_save_to_file(&mgr->cards[i], mgr->cards[i].filepath))
                mgr->cards[i].dirty = false;
        }
    }
}
