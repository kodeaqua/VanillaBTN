#include "sdk_card.h"
#include "libmcrd.h"
#include "libmcx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ps1Card s_internal_cards[2];
static bool s_internal_initialized = false;
static Ps1CardManager *s_bound_manager = NULL;

static long s_last_cmd = 0;
static long s_last_result = McErrNone;
/* Transfers finish synchronously, but expose one consumable SDK completion. */
static bool s_completion_pending;
static long s_mcx_last_cmd = 0;
static long s_mcx_last_result = McxErrInvalid;

void sdk_card_bind_manager(Ps1CardManager *mgr) {
    s_bound_manager = mgr;
}

Ps1Card *sdk_card_get(int port) {
    if (port < 0 || port > 1) {
        return NULL;
    }
    if (s_bound_manager != NULL) {
        return &s_bound_manager->cards[port];
    }
    if (!s_internal_initialized) {
        ps1_card_init(&s_internal_cards[0]);
        ps1_card_init(&s_internal_cards[1]);
        ps1_card_format(&s_internal_cards[0]);
        ps1_card_format(&s_internal_cards[1]);
        s_internal_initialized = true;
    }
    return &s_internal_cards[port];
}

static Ps1Card *get_card_for_chan(int chan) {
    int port = chan == 0 ? 0 : (chan == 0x10 || chan == 1) ? 1 : -1;
    return sdk_card_get(port);
}

void MemCardInit(int val) {
    (void)val;
    sdk_card_get(0);
    s_completion_pending = false;
    s_last_cmd = 0;
    s_last_result = McErrNone;
}

void MemCardEnd(void) {
    s_completion_pending = false;
}

void MemCardStart(void) {
}

void MemCardStop(void) {
    s_completion_pending = false;
}

int MemCardExist(int chan) {
    if (s_completion_pending) return 0;
    Ps1Card *card = get_card_for_chan(chan);
    s_last_cmd = McFuncExist;
    s_last_result = card == NULL ? McErrCardInvalid :
        card->present ? McErrNone : McErrCardNotExist;
    s_completion_pending = true;
    return 1;
}

int MemCardAccept(int chan) {
    if (s_completion_pending) return 0;
    s_completion_pending = true;
    Ps1Card *card = get_card_for_chan(chan);
    s_last_cmd = McFuncAccept;
    if (card == NULL) {
        s_last_result = McErrCardInvalid;
    } else if (!card->present) {
        s_last_result = McErrCardNotExist;
    } else if (card->data[0] != 'M' || card->data[1] != 'C') {
        s_last_result = McErrNotFormat;
    } else if (ps1_card_validate(card) != PS1_CARD_OK) {
        s_last_result = McErrCardInvalid;
    } else {
        s_last_result = McErrNone;
    }
    return 1;
}

int MemCardOpen(int chan, char *file, int flag) {
    (void)chan;
    (void)file;
    (void)flag;
    /* Open/data access is not implemented; never claim a file was opened. */
    return -1;
}

void MemCardClose(void) {
}

int MemCardReadData(unsigned int *adrs, int ofs, int bytes) {
    (void)adrs;
    (void)ofs;
    (void)bytes;
    return 0;
}

/* Resolve the entire on-disk chain before copying anything. This
 * prevents a damaged tail from causing partial reads or corrupting a save. */
static int transfer_file(int chan, char *file, unsigned int *adrs,
                         int ofs, int bytes, bool write) {
    Ps1Card *card = get_card_for_chan(chan);
    uint32_t size = 0;
    uint8_t chain[PS1_CARD_DATA_BLOCKS];
    s_last_cmd = write ? McFuncWriteFile : McFuncReadFile;
    s_last_result = McErrCardInvalid;
    if (!card || !file || !adrs || ofs < 0 || bytes < 0) return -1;
    if (!card->present) {
        s_last_result = McErrCardNotExist;
        return -1;
    }
    Ps1CardStatus status = ps1_card_file_blocks(card, file, chain, &size);
    if (status == PS1_CARD_ERR_NO_FILE) s_last_result = McErrFileNotExist;
    if (status != PS1_CARD_OK || (unsigned)ofs > size ||
        (unsigned)bytes > size - (unsigned)ofs) return -1;
    size_t done = 0;
    while (done < (size_t)bytes) {
        size_t pos = (size_t)ofs + done;
        size_t local = pos % PS1_CARD_BLOCK_SIZE;
        size_t chunk = PS1_CARD_BLOCK_SIZE - local;
        if (chunk > (size_t)bytes - done) chunk = (size_t)bytes - done;
        uint8_t *storage = card->data +
            chain[pos / PS1_CARD_BLOCK_SIZE] * PS1_CARD_BLOCK_SIZE + local;
        uint8_t *buffer = (uint8_t *)adrs + done;
        if (write) memcpy(storage, buffer, chunk);
        else memcpy(buffer, storage, chunk);
        done += chunk;
    }
    if (write && bytes) {
        card->dirty = true;
        /* A completed game save must survive a crash before host shutdown.
         * Unbound cards are deliberately in-memory fixtures. */
        if (s_bound_manager != NULL) {
            if (!ps1_card_save_to_file(card, card->filepath)) return -1;
            card->dirty = false;
        }
    }
    s_last_result = McErrNone;
    return 0;
}

static int register_transfer(int chan, char *file, unsigned int *adrs,
                             int ofs, int bytes, bool write) {
    if (s_completion_pending || !file || !adrs || ofs < 0 || bytes < 0 ||
        bytes % PS1_CARD_FRAME_SIZE != 0) return 0;
    s_completion_pending = true;
    transfer_file(chan, file, adrs, ofs, bytes, write);
    /* Registration succeeds even when the completed operation reports a card
     * or file error. Original save/load obtains that result through Sync. */
    return 1;
}

int MemCardReadFile(int chan, char *file, unsigned int *adrs, int ofs, int bytes) {
    return register_transfer(chan, file, adrs, ofs, bytes, false);
}

int MemCardWriteData(unsigned int *adrs, int ofs, int bytes) {
    (void)adrs;
    (void)ofs;
    (void)bytes;
    return 0;
}

int MemCardWriteFile(int chan, char *file, unsigned int *adrs, int ofs, int bytes) {
    return register_transfer(chan, file, adrs, ofs, bytes, true);
}

/* Metadata operations must remain retryable when disk persistence fails. */
static Ps1Card *stage_card(Ps1Card *card) {
    if (!s_bound_manager) return card;
    Ps1Card *staged = malloc(sizeof(*staged));
    if (staged) *staged = *card;
    return staged;
}

static bool commit_card(Ps1Card *card, Ps1Card *staged) {
    if (card == staged) return true;
    bool saved = ps1_card_save_to_file(staged, staged->filepath);
    if (saved) {
        staged->dirty = false;
        *card = *staged;
    }
    free(staged);
    return saved;
}

int MemCardCreateFile(int chan, char *file, int blocks) {
    Ps1Card *card = get_card_for_chan(chan);
    if (card == NULL || file == NULL || !file[0] || strlen(file) > 20 ||
        blocks <= 0 || blocks > (int)PS1_CARD_DATA_BLOCKS) return -1;
    if (!card->present) return McErrCardNotExist;
    Ps1CardStatus validation = ps1_card_validate(card);
    if (validation == PS1_CARD_ERR_UNFORMATTED) return McErrNotFormat;
    if (validation != PS1_CARD_OK) return McErrCardInvalid;
    /* SDK creation is exclusive. The core replacement API is deliberately
     * stronger and must not erase an existing diary for a create request. */
    if (ps1_card_find_file(card, file, NULL)) return McErrAlreadyExist;
    if (ps1_card_get_free_blocks(card) < blocks) return McErrBlockFull;
    size_t total_size = (size_t)blocks * PS1_CARD_BLOCK_SIZE;
    void *zero_buf = calloc(1, total_size);
    if (!zero_buf) {
        return -1;
    }
    Ps1Card *staged = stage_card(card);
    if (!staged) { free(zero_buf); return McErrCardInvalid; }
    Ps1CardStatus status = ps1_card_write_file(staged, file, zero_buf, total_size);
    free(zero_buf);
    if (status != PS1_CARD_OK) {
        if (staged != card) free(staged);
        return -1;
    }
    return commit_card(card, staged) ? McErrNone : McErrCardInvalid;
}

int MemCardDeleteFile(int chan, char *file) {
    Ps1Card *card = get_card_for_chan(chan);
    if (card == NULL || file == NULL) {
        return -1;
    }
    Ps1Card *staged = stage_card(card);
    if (!staged) return -1;
    Ps1CardStatus status = ps1_card_delete_file(staged, file);
    if (status != PS1_CARD_OK) {
        if (staged != card) free(staged);
        return -1;
    }
    return commit_card(card, staged) ? 0 : -1;
}

int MemCardFormat(int chan) {
    Ps1Card *card = get_card_for_chan(chan);
    if (card == NULL) {
        return -1;
    }
    /* Formatting cannot insert a card, even if its old bytes remain in RAM. */
    if (!card->present) return McErrCardNotExist;
    Ps1Card *staged = stage_card(card);
    if (!staged) return McErrCardInvalid;
    ps1_card_format(staged);
    if (!commit_card(card, staged)) return McErrCardInvalid;
    s_last_cmd = 0x07;
    s_last_result = McErrNone;
    return 0;
}

int MemCardUnformat(int chan) {
    Ps1Card *card = get_card_for_chan(chan);
    if (card == NULL) {
        return -1;
    }
    /* Persistence validates formatted cards; do not claim a disk unformat. */
    if (s_bound_manager != NULL) return -1;
    card->data[0] = 0;
    card->data[1] = 0;
    card->dirty = true;
    s_last_cmd = 0x07;
    s_last_result = McErrNotFormat;
    return 1;
}

int MemCardSync(int mode, long *cmds, long *rslt) {
    if (cmds != NULL) *cmds = s_last_cmd;
    if (rslt != NULL) *rslt = s_last_result;
    if ((mode != 0 && mode != 1) || !s_completion_pending) return -1;
    s_completion_pending = false;
    return 1;
}

static bool pattern_matches(const char *name, const char *pattern) {
    if (pattern == NULL || pattern[0] == '\0' || strcmp(pattern, "*") == 0) {
        return true;
    }
    size_t pat_len = strlen(pattern);
    if (pattern[pat_len - 1] == '*') {
        return strncmp(name, pattern, pat_len - 1) == 0;
    }
    return strcmp(name, pattern) == 0;
}

int MemCardGetDirentry(int chan, const char *name, struct DIRENTRY *dir, long *files, int ofs, int max) {
    if (s_completion_pending) return -1;
    Ps1Card *card = get_card_for_chan(chan);
    long count = 0;

    s_last_cmd = 0x08;
    if (card == NULL || !card->present || ofs < 0 || max < 0 ||
        (max > 0 && dir == NULL) || ps1_card_validate(card) != PS1_CARD_OK) {
        s_last_result = McErrCardInvalid;
        if (files != NULL) {
            *files = 0;
        }
        return -1;
    }

    int skip = ofs;
    for (uint32_t b = 1; b <= PS1_CARD_DATA_BLOCKS; b++) {
        const uint8_t *entry = card->data + b * PS1_CARD_FRAME_SIZE;
        if (entry[0] == PS1_CARD_BLOCK_FIRST) {
            char name_buf[21];
            memcpy(name_buf, entry + 10, 20);
            name_buf[20] = '\0';

            if (pattern_matches(name_buf, name)) {
                if (skip > 0) {
                    skip--;
                    continue;
                }
                if (count >= max) break;
                if (dir != NULL) {
                    memset(&dir[count], 0, sizeof(struct DIRENTRY));
                    memcpy(dir[count].name, name_buf, 20);
                    uint32_t sz;
                    memcpy(&sz, entry + 4, 4);
                    dir[count].size = (long)sz;
                    dir[count].head = (long)b;
                }
                count++;
            }
        }
    }

    if (files != NULL) {
        *files = count;
    }
    s_last_result = McErrNone;
    return 0;
}

/* A virtual PS1 card is not a PocketStation. Complete the SDK request with
 * the real absence/type error; never invent PDA memory, identity or clock data.
 * Immediate return 1 means the request was accepted, not that it succeeded. */
static int mcx_unavailable(long port, long command) {
    int slot = port == 0 ? 0 : (port == 1 || port == 0x10) ? 1 : -1;
    Ps1Card *card = sdk_card_get(slot);
    s_mcx_last_cmd = command;
    s_mcx_last_result = slot < 0 ? McxErrInvalid
        : (!card || !card->present) ? McxErrNoCard : McxErrInvalid;
    return 1;
}

int McxCardType(long port) {
    return mcx_unavailable(port, McxFuncCardType);
}

int McxSync(long mode, long *cmd, long *result) {
    if (mode != 0 && mode != 1) return -1;
    if (cmd) *cmd = s_mcx_last_cmd;
    if (result) *result = s_mcx_last_result;
    return 1;
}

int McxAllInfo(long port, unsigned char *buf) {
    (void)buf;
    return mcx_unavailable(port, McxFuncAllInfo);
}

int McxGetApl(long port, void *result) {
    (void)result;
    return mcx_unavailable(port, McxFuncGetApl);
}

int McxGetTime(long port, unsigned char *buf) {
    (void)buf;
    return mcx_unavailable(port, McxFuncGetTime);
}

int McxHideTrans(long port) {
    return mcx_unavailable(port, McxFuncHideTrans);
}

int McxGetSerial(long port, void *serial) {
    (void)serial;
    return mcx_unavailable(port, McxFuncGetSerial);
}

int McxGetMem(long port, void *buf, long addr, long len) {
    (void)buf;
    (void)addr;
    (void)len;
    return mcx_unavailable(port, McxFuncGetMem);
}

int McxExecFlag(long port, long block, long flag) {
    (void)block;
    (void)flag;
    return mcx_unavailable(port, McxFuncExecFlag);
}

int McxExecApl(long port, long apl, long param) {
    (void)apl;
    (void)param;
    return mcx_unavailable(port, McxFuncExecApl);
}

int McxCurrCtrl(long port, long sound, long infrared, long rsv) {
    (void)sound;
    (void)infrared;
    (void)rsv;
    return mcx_unavailable(port, McxFuncCurrCtrl);
}

int McxFlashAcs(long port, long flag) {
    (void)flag;
    return mcx_unavailable(port, McxFuncFlashAcs);
}

int McxSetMem(long port, const void *buf, long addr, long len) {
    (void)buf;
    (void)addr;
    (void)len;
    return mcx_unavailable(port, McxFuncSetMem);
}

int McxShowTrans(long port, long shown, long timeout) {
    (void)shown;
    (void)timeout;
    return mcx_unavailable(port, McxFuncShowTrans);
}

int McxSetLED(long port, long flag) {
    (void)flag;
    return mcx_unavailable(port, McxFuncSetLED);
}

int McxReadDev(long port, long dev, const void *fix, void *read) {
    (void)dev;
    (void)fix;
    (void)read;
    return mcx_unavailable(port, McxFuncReadDev);
}

int McxWriteDev(long port, long dev, const void *fix, const void *read) {
    (void)dev;
    (void)fix;
    (void)read;
    return mcx_unavailable(port, McxFuncWriteDev);
}

int sdk_card_rename(const char *oldpath, const char *newpath) {
    if (oldpath == NULL || newpath == NULL) {
        return -1;
    }
    int port = 0;
    const char *colon_old = strchr(oldpath, ':');
    const char *colon_new = strchr(newpath, ':');
    const char *oldname = (colon_old != NULL) ? colon_old + 1 : oldpath;
    const char *newname = (colon_new != NULL) ? colon_new + 1 : newpath;

    if (oldpath[0] == 'b' && oldpath[1] == 'u') {
        port = oldpath[2] - '0';
    }
    Ps1Card *card = sdk_card_get(port);
    if (card == NULL || !card->present || ps1_card_validate(card) != PS1_CARD_OK) {
        return -1;
    }

    for (uint32_t b = 1; b <= PS1_CARD_DATA_BLOCKS; b++) {
        uint8_t *entry = card->data + b * PS1_CARD_FRAME_SIZE;
        if (entry[0] == PS1_CARD_BLOCK_FIRST) {
            char name_buf[21];
            memcpy(name_buf, entry + 10, 20);
            name_buf[20] = '\0';
            if (strcmp(name_buf, oldname) == 0) {
                Ps1Card *staged = stage_card(card);
                if (!staged) return -1;
                entry = staged->data + b * PS1_CARD_FRAME_SIZE;
                memset(entry + 10, 0, 20);
                strncpy((char *)(entry + 10), newname, 20);
                entry[127] = ps1_card_checksum(entry);
                staged->dirty = true;
                return commit_card(card, staged) ? 1 : -1;
            }
        }
    }
    return -1;
}

void McxStartCom(void) {
    s_mcx_last_cmd = 0;
    s_mcx_last_result = McxErrInvalid;
}
