/* Keep checks active in the optimized build. */
#undef NDEBUG
#include <assert.h>
#include <string.h>
#include <windows.h>
#include "card_file.h"

enum { HALF_SIZE = 128, SLOT_SIZE = 2 * HALF_SIZE };

static void read_slot(HANDLE handle, unsigned char slot[SLOT_SIZE]) {
    DWORD count;
    assert(ReadFile(handle, slot, SLOT_SIZE, &count, NULL) && count == SLOT_SIZE);
}

int main(void) {
    char path[MAX_PATH];
    unsigned char original[SLOT_SIZE];
    unsigned char replacement[HALF_SIZE];
    unsigned char result[SLOT_SIZE];
    DWORD count;

    memset(original, 0x11, sizeof(original));
    memset(replacement, 0x22, sizeof(replacement));
    assert(GetTempFileNameA(".", "vbt", 0, path));
    HANDLE handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    assert(handle != INVALID_HANDLE_VALUE);
    assert(WriteFile(handle, original, SLOT_SIZE, &count, NULL) && count == SLOT_SIZE);
    assert(CloseHandle(handle));

    /* Readers can share the old file while a replacement is blocked. */
    handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         OPEN_EXISTING, 0, NULL);
    assert(handle != INVALID_HANDLE_VALUE);
    assert(card_write_file(path, replacement, HALF_SIZE, HALF_SIZE) == CARD_WRITE_FAILED);
    read_slot(handle, result);
    assert(memcmp(original, result, SLOT_SIZE) == 0);
    assert(CloseHandle(handle));

    assert(card_write_file(path, replacement, HALF_SIZE, HALF_SIZE) == CARD_WRITE_OK);
    handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    assert(handle != INVALID_HANDLE_VALUE);
    read_slot(handle, result);
    assert(memcmp(original, result, HALF_SIZE) == 0);
    assert(memcmp(replacement, result + HALF_SIZE, HALF_SIZE) == 0);
    assert(CloseHandle(handle));

    assert(card_write_file(path, original, SLOT_SIZE - HALF_SIZE / 2, HALF_SIZE) ==
           CARD_WRITE_FAILED);
    handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    assert(handle != INVALID_HANDLE_VALUE);
    assert(GetFileSize(handle, NULL) == SLOT_SIZE);
    read_slot(handle, result);
    assert(memcmp(original, result, HALF_SIZE) == 0);
    assert(memcmp(replacement, result + HALF_SIZE, HALF_SIZE) == 0);
    assert(CloseHandle(handle));

    assert(DeleteFileA(path));
    assert(card_write_file(path, replacement, HALF_SIZE, HALF_SIZE) == CARD_WRITE_MISSING);
    assert(card_write_file(NULL, replacement, 0, HALF_SIZE) == CARD_WRITE_FAILED);
    return 0;
}
