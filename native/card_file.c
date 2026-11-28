#include <string.h>
#include <windows.h>
#include "card_file.h"

enum CardWriteResult card_write_file(const char *path, const void *data, long offset,
                                     unsigned long size) {
    char directory[MAX_PATH];
    if (path == NULL || data == NULL || offset < 0 || size == 0 ||
        strcpy_s(directory, sizeof(directory), path) != 0) {
        return CARD_WRITE_FAILED;
    }
    char *name = strrchr(directory, '\\');
    if (name == NULL) {
        return CARD_WRITE_FAILED;
    }
    *name = '\0';

    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? CARD_WRITE_MISSING
                                                                            : CARD_WRITE_FAILED;
    }
    if (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY)) {
        return CARD_WRITE_FAILED;
    }

    /* PsyZ ignores dot-prefixed files when it enumerates card slots. */
    char staged[MAX_PATH];
    if (GetTempFileNameA(directory, ".vb", 0, staged) == 0) {
        return CARD_WRITE_FAILED;
    }

    BOOL written = FALSE;
    if (CopyFileA(path, staged, FALSE)) {
        HANDLE handle = CreateFileA(staged, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (handle != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER capacity;
            LARGE_INTEGER position;
            DWORD count;
            position.QuadPart = offset;
            written = GetFileSizeEx(handle, &capacity) &&
                      position.QuadPart + size <= capacity.QuadPart &&
                      SetFilePointerEx(handle, position, NULL, FILE_BEGIN) &&
                      WriteFile(handle, data, size, &count, NULL) && count == size &&
                      FlushFileBuffers(handle);
            written = CloseHandle(handle) && written;
        }
    }
    if (written && MoveFileExA(staged, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return CARD_WRITE_OK;
    }
    DeleteFileA(staged);
    return CARD_WRITE_FAILED;
}
