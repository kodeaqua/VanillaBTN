/* ps1_card.h — host-side virtual PS1 memory card system.
 *
 * Implements:
 *   - Two standard 128 KB (131 072 bytes) virtual memory card slots.
 *   - Standard PS1 Memory Card image layout (compatible with .mcr / .mcd files
 *     from emulators or raw hardware exports; container headers are not parsed).
 *   - Frame 0 header validation, directory frame (1..15) block allocation chains,
 *     and XOR frame checksums.
 *   - HM:BTN save file format: 4 blocks (0x8000 = 32 768 bytes), filename
 *     "BASLUS-01115-FARM<x>", header block at 0x200 with checksum.
 *   - Safe atomic file operations: write-to-temp, validate, backup existing,
 *     replace, and automatic interrupted-write crash recovery.
 *   - Direct import/export of raw save files and entire memory card images.
 */

#ifndef PS1_CARD_H
#define PS1_CARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PS1_CARD_SIZE               131072u  /* 128 KB */
#define PS1_CARD_FRAME_SIZE         128u
#define PS1_CARD_FRAMES_PER_BLOCK   64u
#define PS1_CARD_BLOCK_SIZE         (PS1_CARD_FRAME_SIZE * PS1_CARD_FRAMES_PER_BLOCK) /* 8192 bytes */
#define PS1_CARD_TOTAL_BLOCKS       16u      /* Block 0 = directory, Blocks 1..15 = data */
#define PS1_CARD_DATA_BLOCKS        15u      /* Usable data blocks */

#define PS1_CARD_SAVE_BLOCKS        4u       /* HM:BTN uses 4 blocks per save */
#define PS1_CARD_SAVE_SIZE          (PS1_CARD_SAVE_BLOCKS * PS1_CARD_BLOCK_SIZE) /* 32768 bytes (0x8000) */
#define PS1_CARD_SAVE_SLOTS         3        /* 3 farm diary slots (FARM0, FARM1, FARM2) */

#define PS1_CARD_HEADER_OFFSET      0x200u   /* Offset of SaveSlotRecord inside save file */
#define PS1_CARD_HEADER_SIZE        0x80u    /* 128 bytes checked with XOR checksum at +0x7F */

/* Block allocation states */
#define PS1_CARD_BLOCK_FIRST        0x51u
#define PS1_CARD_BLOCK_MIDDLE       0x52u
#define PS1_CARD_BLOCK_LAST         0x53u
#define PS1_CARD_BLOCK_FREE         0xA0u

/* Card status codes (matching saveload.c semantics) */
typedef enum {
    PS1_CARD_OK = 0,
    PS1_CARD_ERR_NO_CARD = 1,
    PS1_CARD_ERR_UNFORMATTED = 2,
    PS1_CARD_ERR_DAMAGED = 3,
    PS1_CARD_ERR_NO_FILE = 4,
    PS1_CARD_ERR_FULL = 5,
    PS1_CARD_ERR_BAD_CHECKSUM = 6,
    PS1_CARD_ERR_IO = 7
} Ps1CardStatus;

/* Virtual Memory Card representation */
typedef struct {
    uint8_t data[PS1_CARD_SIZE];
    bool    present;
    bool    dirty;
    char    filepath[512];
} Ps1Card;

/* Dual-slot Memory Card Manager */
typedef struct {
    Ps1Card cards[2];
    char    base_dir[512];
} Ps1CardManager;

/* -------------------------------------------------------------------------
 * Low-level card operations
 * ---------------------------------------------------------------------- */
void ps1_card_init(Ps1Card *card);

/* Formats card image with valid Frame 0 ("MC") and 15 free directory frames */
void ps1_card_format(Ps1Card *card);

/* Invalidate only the card signature, leaving files intact for a future
 * raw-image recovery. Returns false for missing/invalid/already blank cards. */
bool ps1_card_unformat(Ps1Card *card);

/* True only for an explicitly unformatted image whose original directory
 * and checksum would validate if the two signature bytes were restored. */
bool ps1_card_is_unformatted(const Ps1Card *card);

/* Validate magic, checksums, chain ownership, links and allocation states. */
Ps1CardStatus ps1_card_validate(const Ps1Card *card);

/* Calculates 1-byte XOR checksum of 127 bytes */
uint8_t ps1_card_checksum(const uint8_t *frame128);

/* Get number of free 8KB blocks available on card */
int ps1_card_get_free_blocks(const Ps1Card *card);

/* Find a file on the card by name.
 * If found, returns start block (1..15) and sets *out_size. Returns 0 if not found. */
int ps1_card_find_file(const Ps1Card *card, const char *filename, uint32_t *out_size);

/* Resolve a validated file into physical block numbers (1..15), converting
 * the on-disk zero-based links. Outputs remain untouched on failure. The
 * number of output blocks is ceil(*out_size / PS1_CARD_BLOCK_SIZE). */
Ps1CardStatus ps1_card_file_blocks(const Ps1Card *card, const char *filename,
    uint8_t blocks[PS1_CARD_DATA_BLOCKS], uint32_t *out_size);

/* Read allocated file bytes (whole blocks, optionally limited by max_size). */
Ps1CardStatus ps1_card_read_file(const Ps1Card *card, const char *filename,
                                 void *out_buf, size_t max_size, size_t *out_size);

/* Write file payload to card. Allocates contiguous blocks, updates directory frames. */
/* Allocates whole blocks and zero-pads the supplied payload to that size. */
Ps1CardStatus ps1_card_write_file(Ps1Card *card, const char *filename,
                                  const void *data, size_t size);

/* Delete file from card by marking its directory frames free */
Ps1CardStatus ps1_card_delete_file(Ps1Card *card, const char *filename);

/* -------------------------------------------------------------------------
 * Harvest Moon: Back to Nature specific helpers (slots 0..2)
 * ---------------------------------------------------------------------- */
/* Returns canonical filename for slot 0..2 ("BASLUS-01115-FARM0", etc.) */
const char *ps1_card_get_farm_filename(int slot_idx);

/* Reads 32KB farm save for slot 0..2 */
Ps1CardStatus ps1_card_read_farm_slot(const Ps1Card *card, int slot_idx,
                                      void *out_buf, size_t max_size, size_t *out_size);

/* Writes 32KB farm save for slot 0..2 */
Ps1CardStatus ps1_card_write_farm_slot(Ps1Card *card, int slot_idx,
                                       const void *data, size_t size);

/* Reads and verifies the 128-byte slot header record at offset 0x200 */
Ps1CardStatus ps1_card_read_slot_header(const Ps1Card *card, int slot_idx,
                                        void *out_header, size_t header_size);

/* -------------------------------------------------------------------------
 * File persistence, atomic write, and crash recovery
 * ---------------------------------------------------------------------- */
/* Load an exact raw card image. Only a missing file creates a fresh card.
 * Explicitly unformatted images are accepted and stay unformatted.
 * Invalid existing images remain unchanged and clean; unreadable/wrong-size
 * images are unavailable. Creation failure is reported, never fabricated. */
bool ps1_card_load_from_file(Ps1Card *card, const char *filepath);

/* Atomically saves card to disk:
 * 1. Writes to filepath.tmp
 * 2. Validates written temp file
 * 3. Rotates existing backups (.bak -> .bak.1 -> .bak.2 ...)
 * 4. Backs up existing file to filepath.bak
 * 5. Replaces filepath with temp file
 */
bool ps1_card_save_to_file(const Ps1Card *card, const char *filepath);

/* Rotates multi-generational backups up to max_backups */
bool ps1_card_rotate_backups(const char *filepath, int max_backups);

/* Recovers the most recent valid or explicitly unformatted .tmp, .bak.hold
 * or .bak card. If these are
 * missing/damaged, tries rotated .bak.1 through .bak.5 in newest-first order.
 * Unrecoverable backup generations block implicit blank-card creation. */
bool ps1_card_recover_file(const char *filepath);

/* -------------------------------------------------------------------------
 * Card Manager Lifecycle
 * ---------------------------------------------------------------------- */
/* Initialize manager with an existing custom directory, or use SDL_GetPrefPath
 * when custom_dir is NULL/empty. An unusable explicit directory is rejected. */
bool ps1_card_manager_init(Ps1CardManager *mgr, const char *custom_dir);

/* Flush dirty cards and shutdown */
void ps1_card_manager_shutdown(Ps1CardManager *mgr);

#endif /* PS1_CARD_H */
