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
    int port = (chan == 0x10 || chan == 1) ? 1 : 0;
    return sdk_card_get(port);
}

void MemCardInit(int val) {
    (void)val;
    sdk_card_get(0);
}

void MemCardEnd(void) {
}

void MemCardStart(void) {
}

void MemCardStop(void) {
}

int MemCardExist(int chan) {
    Ps1Card *card = get_card_for_chan(chan);
    return (card != NULL && card->present) ? 1 : 0;
}

int MemCardAccept(int chan) {
    Ps1Card *card = get_card_for_chan(chan);
    s_last_cmd = McFuncAccept;
    if (card == NULL || !card->present) {
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
    return 0;
}

void MemCardClose(void) {
}

int MemCardReadData(unsigned int *adrs, int ofs, int bytes) {
    (void)adrs;
    (void)ofs;
    (void)bytes;
    return 0;
}

int MemCardReadFile(int chan, char *file, unsigned int *adrs, int ofs, int bytes) {
    Ps1Card *card = get_card_for_chan(chan);
    uint32_t file_size = 0;
    int start_block;
    int curr_block;
    size_t block_ofs = 0;
    size_t bytes_read = 0;
    uint8_t *dst = (uint8_t *)adrs;

    s_last_cmd = McFuncReadFile;
    if (card == NULL || file == NULL || adrs == NULL) {
        s_last_result = McErrCardInvalid;
        return -1;
    }

    start_block = ps1_card_find_file(card, file, &file_size);
    if (start_block == 0) {
        s_last_result = McErrFileNotExist;
        return -1;
    }

    curr_block = start_block;
    while (curr_block >= 1 && curr_block <= (int)PS1_CARD_DATA_BLOCKS && bytes_read < (size_t)bytes) {
        if ((size_t)ofs >= block_ofs + PS1_CARD_BLOCK_SIZE) {
            const uint8_t *df = card->data + curr_block * PS1_CARD_FRAME_SIZE;
            uint16_t next;
            memcpy(&next, df + 8, 2);
            if (next == 0xFFFF) {
                break;
            }
            curr_block = (int)next;
            block_ofs += PS1_CARD_BLOCK_SIZE;
            continue;
        }

        size_t in_block_start = (ofs > (int)block_ofs) ? ((size_t)ofs - block_ofs) : 0;
        size_t in_block_avail = PS1_CARD_BLOCK_SIZE - in_block_start;
        size_t chunk = ((size_t)bytes - bytes_read < in_block_avail) ? ((size_t)bytes - bytes_read) : in_block_avail;
        const uint8_t *src = card->data + (size_t)curr_block * PS1_CARD_BLOCK_SIZE + in_block_start;

        memcpy(dst + bytes_read, src, chunk);
        bytes_read += chunk;

        const uint8_t *df = card->data + curr_block * PS1_CARD_FRAME_SIZE;
        uint16_t next;
        memcpy(&next, df + 8, 2);
        if (next == 0xFFFF) {
            break;
        }
        curr_block = (int)next;
        block_ofs += PS1_CARD_BLOCK_SIZE;
    }

    s_last_result = McErrNone;
    return 0;
}

int MemCardWriteData(unsigned int *adrs, int ofs, int bytes) {
    (void)adrs;
    (void)ofs;
    (void)bytes;
    return 0;
}

int MemCardWriteFile(int chan, char *file, unsigned int *adrs, int ofs, int bytes) {
    Ps1Card *card = get_card_for_chan(chan);
    uint32_t file_size = 0;
    int start_block;
    int curr_block;
    size_t block_ofs = 0;
    size_t bytes_written = 0;
    const uint8_t *src = (const uint8_t *)adrs;

    s_last_cmd = McFuncWriteFile;
    if (card == NULL || file == NULL || adrs == NULL) {
        s_last_result = McErrCardInvalid;
        return -1;
    }

    start_block = ps1_card_find_file(card, file, &file_size);
    if (start_block == 0) {
        s_last_result = McErrFileNotExist;
        return -1;
    }

    curr_block = start_block;
    while (curr_block >= 1 && curr_block <= (int)PS1_CARD_DATA_BLOCKS && bytes_written < (size_t)bytes) {
        if ((size_t)ofs >= block_ofs + PS1_CARD_BLOCK_SIZE) {
            const uint8_t *df = card->data + curr_block * PS1_CARD_FRAME_SIZE;
            uint16_t next;
            memcpy(&next, df + 8, 2);
            if (next == 0xFFFF) {
                break;
            }
            curr_block = (int)next;
            block_ofs += PS1_CARD_BLOCK_SIZE;
            continue;
        }

        size_t in_block_start = (ofs > (int)block_ofs) ? ((size_t)ofs - block_ofs) : 0;
        size_t in_block_avail = PS1_CARD_BLOCK_SIZE - in_block_start;
        size_t chunk = ((size_t)bytes - bytes_written < in_block_avail) ? ((size_t)bytes - bytes_written) : in_block_avail;
        uint8_t *dst = card->data + (size_t)curr_block * PS1_CARD_BLOCK_SIZE + in_block_start;

        memcpy(dst, src + bytes_written, chunk);
        bytes_written += chunk;

        const uint8_t *df = card->data + curr_block * PS1_CARD_FRAME_SIZE;
        uint16_t next;
        memcpy(&next, df + 8, 2);
        if (next == 0xFFFF) {
            break;
        }
        curr_block = (int)next;
        block_ofs += PS1_CARD_BLOCK_SIZE;
    }

    card->dirty = true;
    s_last_result = McErrNone;
    return 0;
}

int MemCardCreateFile(int chan, char *file, int blocks) {
    Ps1Card *card = get_card_for_chan(chan);
    if (card == NULL || file == NULL || blocks <= 0) {
        return -1;
    }
    size_t total_size = (size_t)blocks * PS1_CARD_BLOCK_SIZE;
    void *zero_buf = calloc(1, total_size);
    if (!zero_buf) {
        return -1;
    }
    Ps1CardStatus status = ps1_card_write_file(card, file, zero_buf, total_size);
    free(zero_buf);
    return (status == PS1_CARD_OK) ? 0 : -1;
}

int MemCardDeleteFile(int chan, char *file) {
    Ps1Card *card = get_card_for_chan(chan);
    if (card == NULL || file == NULL) {
        return -1;
    }
    Ps1CardStatus status = ps1_card_delete_file(card, file);
    return (status == PS1_CARD_OK) ? 0 : -1;
}

int MemCardFormat(int chan) {
    Ps1Card *card = get_card_for_chan(chan);
    if (card == NULL) {
        return -1;
    }
    ps1_card_format(card);
    s_last_cmd = 0x07;
    s_last_result = McErrNone;
    return 0;
}

int MemCardUnformat(int chan) {
    Ps1Card *card = get_card_for_chan(chan);
    if (card == NULL) {
        return -1;
    }
    card->data[0] = 0;
    card->data[1] = 0;
    card->dirty = true;
    s_last_cmd = 0x07;
    s_last_result = McErrNotFormat;
    return 1;
}

int MemCardSync(int mode, long *cmds, long *rslt) {
    (void)mode;
    if (cmds != NULL) {
        *cmds = s_last_cmd;
    }
    if (rslt != NULL) {
        *rslt = s_last_result;
    }
    return 0;
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
    Ps1Card *card = get_card_for_chan(chan);
    long count = 0;

    s_last_cmd = 0x08;
    if (card == NULL || ps1_card_validate(card) != PS1_CARD_OK) {
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
                if (count < max && dir != NULL) {
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

int McxCardType(long port) {
    int p = (port == 0x10 || port == 1) ? 1 : 0;
    Ps1Card *card = sdk_card_get(p);
    s_mcx_last_cmd = 1;
    if (card == NULL || !card->present) {
        s_mcx_last_result = McxErrNoCard;
    } else {
        s_mcx_last_result = McxErrInvalid; /* Standard card, not a PocketStation PDA */
    }
    return 0;
}

int McxSync(long port, long *cmd, long *result) {
    (void)port;
    if (cmd != NULL) {
        *cmd = s_mcx_last_cmd;
    }
    if (result != NULL) {
        *result = s_mcx_last_result;
    }
    return 1;
}

int McxAllInfo(long port, unsigned char *buf) {
    (void)port;
    if (buf != NULL) {
        memset(buf, 0, 36);
        buf[2] = 1;
        buf[3] = 0;
        buf[4] = 0x78;
        buf[5] = 0x56;
        buf[6] = 0x34;
        buf[7] = 0x12;
    }
    s_mcx_last_cmd = 0x01;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxGetApl(long port, void *result) {
    (void)port;
    if (result != NULL) {
        *(long *)result = 0;
    }
    s_mcx_last_cmd = 0x02;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxGetTime(long port, unsigned char *buf) {
    (void)port;
    if (buf != NULL) {
        memset(buf, 0, 8);
        buf[0] = 0x20;
        buf[1] = 0x26;
        buf[2] = 11;
        buf[3] = 8;
        buf[4] = 0;
        buf[5] = 12;
        buf[6] = 0;
        buf[7] = 0;
    }
    s_mcx_last_cmd = 0x03;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxHideTrans(long port) {
    (void)port;
    s_mcx_last_cmd = 0x04;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxGetSerial(long port, void *serial) {
    (void)port;
    if (serial != NULL) {
        *(long *)serial = 0x12345678;
    }
    s_mcx_last_cmd = 0x05;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxGetMem(long port, void *buf, long addr, long len) {
    (void)port;
    (void)addr;
    if (buf != NULL && len > 0) {
        memset(buf, 0, (size_t)len);
    }
    s_mcx_last_cmd = 0x06;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxExecFlag(long port, long block, long flag) {
    (void)port;
    (void)block;
    (void)flag;
    s_mcx_last_cmd = McxFuncExecFlag;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxExecApl(long port, long apl, long param) {
    (void)port;
    (void)apl;
    (void)param;
    s_mcx_last_cmd = 0x07;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxCurrCtrl(long port, long sound, long infrared, long rsv) {
    (void)port;
    (void)sound;
    (void)infrared;
    (void)rsv;
    s_mcx_last_cmd = 0x08;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxFlashAcs(long port, long flag) {
    (void)port;
    (void)flag;
    s_mcx_last_cmd = 0x09;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxSetMem(long port, const void *buf, long addr, long len) {
    (void)port;
    (void)buf;
    (void)addr;
    (void)len;
    s_mcx_last_cmd = 0x0A;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxShowTrans(long port, long shown, long timeout) {
    (void)port;
    (void)shown;
    (void)timeout;
    s_mcx_last_cmd = 0x0B;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxSetLED(long port, long flag) {
    (void)port;
    (void)flag;
    s_mcx_last_cmd = 0x0C;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxReadDev(long port, long dev, const void *fix, void *read) {
    (void)port;
    (void)dev;
    (void)fix;
    if (read != NULL) {
        memset(read, 0, 128);
    }
    s_mcx_last_cmd = 0x0D;
    s_mcx_last_result = McxErrSuccess;
    return 1;
}

int McxWriteDev(long port, long dev, const void *fix, const void *read) {
    (void)port;
    (void)dev;
    (void)fix;
    (void)read;
    s_mcx_last_cmd = 0x0E;
    s_mcx_last_result = McxErrSuccess;
    return 1;
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
                memset(entry + 10, 0, 20);
                strncpy((char *)(entry + 10), newname, 20);
                entry[127] = ps1_card_checksum(entry);
                card->dirty = true;
                return 1; /* 1 = success in PS1 rename */
            }
        }
    }
    return -1;
}
