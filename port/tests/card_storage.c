#include "psx/card.h"
#include <libmcrd.h>
#include <libmcx.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
enum { CARD_BYTES = 512 + 15 * 8192 };
static unsigned char disk[2][CARD_BYTES];
static int present[2], fail_write;
static int read_card(void *context, int slot, void *data, size_t bytes) {
    (void)context;
    if (bytes != CARD_BYTES)
        return -1;
    if (!present[slot])
        return 0;
    memcpy(data, disk[slot], bytes);
    return 1;
}
static int write_card(void *context, int slot, const void *data, size_t bytes) {
    (void)context;
    if (bytes != CARD_BYTES || fail_write)
        return -1;
    memcpy(disk[slot], data, bytes);
    present[slot] = 1;
    return 1;
}
static int complete(int expected_command, int expected_result) {
    s32 command = -1, result = -1;
    if (MemCardSync(1, &command, &result) != 1 || command != expected_command ||
        result != expected_result)
        return 0;
    return MemCardSync(1, &command, &result) == -1;
}
int main(void) {
    hm_card_storage store = {NULL, read_card, write_card};
    hm_card_attach(store);
    MemCardInit(0);
    MemCardStart();
    McxStartCom();
    CHECK(present[0] && present[1]);
    CHECK(MemCardAccept(0) == 1 && complete(1, McErrNone));
    CHECK(McxCardType(0) == 1);
    s32 command, result;
    CHECK(McxSync(0, &command, &result) == 1 && result == McxErrInvalid);
    CHECK(MemCardCreateFile(0, "SAVE01", 4) == McErrNone);
    CHECK(MemCardCreateFile(0, "SAVE01", 4) == McErrAlreadyExist);
    CHECK(MemCardCreateFile(0, "../bad", 1) == McErrCardInvalid);
    CHECK(MemCardCreateFile(0, "too-big", 16) == McErrCardInvalid);
    CHECK(MemCardCreateFile(0, "SAVE02", 4) == McErrNone);
    CHECK(MemCardCreateFile(0, "SAVE03", 4) == McErrNone);
    CHECK(MemCardCreateFile(0, "SAVE04", 4) == McErrBlockFull);
    CHECK(MemCardCreateFile(0x10, "SAVE01", 4) == McErrNone);
    struct DIRENTRY entries[15];
    s32 count;
    CHECK(MemCardGetDirentry(0, "*", entries, &count, 0, 15) == 0 && count == 3);
    CHECK(!strcmp(entries[0].name, "SAVE01") && entries[0].size == 32768);
    CHECK(MemCardGetDirentry(0, "SAVE0?", entries, &count, 1, 1) == 0 && count == 1);
    CHECK(!strcmp(entries[0].name, "SAVE02"));
    CHECK(MemCardGetDirentry(0, "missing", entries, &count, 0, 15) == 0 && count == 0);
    unsigned char saved[32768], loaded[32768];
    for (size_t i = 0; i < sizeof(saved); ++i)
        saved[i] = (unsigned char)(i * 17u + i / 71u);
    CHECK(MemCardWriteFile(0, "SAVE01", saved, 0, sizeof(saved)) == 1);
    CHECK(MemCardReadFile(0, "SAVE01", loaded, 0, 1) == 0); /* Completion is pending. */
    CHECK(complete(4, McErrNone));
    hm_card_detach();
    hm_card_attach(store); /* Reopen the persisted bytes, not the old RAM copy. */
    MemCardInit(0);
    MemCardStart();
    CHECK(MemCardReadFile(0, "SAVE01", loaded, 0, sizeof(loaded)) == 1);
    CHECK(complete(3, McErrNone) && !memcmp(saved, loaded, sizeof(saved)));
    CHECK(MemCardReadFile(0x10, "SAVE01", loaded, 0, sizeof(loaded)) == 1);
    CHECK(complete(3, McErrNone) && loaded[111] == 0); /* Slot isolation. */
    CHECK(MemCardReadFile(0, "SAVE01", loaded, 32760, 9) == 1);
    CHECK(complete(3, McErrCardInvalid));
    CHECK(MemCardReadFile(0, "missing", loaded, 0, 128) == 1);
    CHECK(complete(3, McErrFileNotExist));
    fail_write = 1;
    unsigned char change = 99;
    CHECK(MemCardWriteFile(0, "SAVE01", &change, 111, 1) == 1);
    CHECK(complete(4, McErrCardInvalid));
    CHECK(MemCardReadFile(0, "SAVE01", loaded, 0, sizeof(loaded)) == 1);
    CHECK(complete(3, McErrNone) && !memcmp(saved, loaded, sizeof(saved)));
    CHECK(MemCardCreateFile(0x10, "FAILED", 1) == McErrCardInvalid);
    CHECK(MemCardGetDirentry(0x10, "FAILED", entries, &count, 0, 15) == 0 && count == 0);
    fail_write = 0;
    CHECK(MemCardWriteFile(0, "SAVE01", &change, 111, 1) == 1 && complete(4, 0));
    CHECK(MemCardReadFile(0, "SAVE01", loaded, 0, sizeof(loaded)) == 1 && complete(3, 0));
    saved[111] = change;
    CHECK(!memcmp(saved, loaded, sizeof(saved)));
    CHECK(MemCardUnformat(0x10) == McErrNone);
    CHECK(MemCardAccept(0x10) == 1 && complete(1, McErrNotFormat));
    CHECK(MemCardFormat(0x10) == McErrNone);
    CHECK(MemCardGetDirentry(0x10, "*", entries, &count, 0, 15) == 0 && count == 0);
    disk[0][0] ^= 1; /* Corruption is reported, never silently reformatted. */
    hm_card_attach(store);
    CHECK(MemCardAccept(0) == 1 && complete(1, McErrCardInvalid));
    CHECK(disk[0][0] != 'H');
    hm_card_detach();
    CHECK(MemCardAccept(0) == 1 && complete(1, McErrCardNotExist));
    return 0;
}
