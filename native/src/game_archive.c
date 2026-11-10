#include "game_archive.h"

#include <string.h>

static uint32_t read_u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8
        | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

void game_archive_close(GameArchive *archive) {
    if (archive == NULL) return;
    if (archive->file != NULL) fclose(archive->file);
    memset(archive, 0, sizeof(*archive));
}

bool game_archive_open(GameArchive *archive, const char *directory) {
    char path[1024];
    unsigned char header[132];
    FILE *index;
    long length;
    int written;
    size_t dlen;
    const char *sep;
    if (archive == NULL || directory == NULL) return false;
    memset(archive, 0, sizeof(*archive));
    dlen = strlen(directory);
    sep = (dlen > 0 && (directory[dlen - 1] == '/' || directory[dlen - 1] == '\\')) ? "" : "/";
    written = snprintf(path, sizeof(path), "%s%sA_FILE.HDT", directory, sep);
    if (written < 0 || (size_t)written >= sizeof(path)) return false;
    index = fopen(path, "rb");
    if (index == NULL) return false;
    bool valid = fread(header, 1, sizeof(header), index) == sizeof(header)
        && fgetc(index) == EOF && !ferror(index);
    fclose(index);
    if (!valid) return false;
    for (unsigned i = 0; i < 33; ++i) {
        archive->offsets[i] = read_u32(header + i * 4);
        if ((archive->offsets[i] & 2047u) != 0
            || (i == 0 && archive->offsets[i] != 0)
            || (i != 0 && archive->offsets[i] < archive->offsets[i - 1])) {
            return false;
        }
    }
    written = snprintf(path, sizeof(path), "%s%sA_FILE.BIN", directory, sep);
    if (written < 0 || (size_t)written >= sizeof(path)) return false;
    archive->file = fopen(path, "rb");
    if (archive->file == NULL) return false;
    /* The USA archive is smaller than LONG_MAX on Windows. */
    if (fseek(archive->file, 0, SEEK_END) != 0
        || (length = ftell(archive->file)) < 0
        || (uint32_t)length != archive->offsets[32]) {
        game_archive_close(archive);
        return false;
    }
    return true;
}

bool game_archive_read(GameArchive *archive, unsigned member, uint32_t offset,
                       void *destination, uint32_t size) {
    uint32_t length;
    if (archive == NULL || archive->file == NULL || member >= 32
        || destination == NULL) return false;
    length = archive->offsets[member + 1] - archive->offsets[member];
    /* Subtraction avoids overflow when validating untrusted offsets. */
    if (offset > length || size > length - offset) return false;
    return fseek(archive->file, (long)(archive->offsets[member] + offset), SEEK_SET) == 0
        && fread(destination, 1, size, archive->file) == size;
}
