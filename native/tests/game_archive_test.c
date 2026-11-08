#include "game_archive.h"

#include <assert.h>
#include <string.h>

static void write_fixture(bool corrupt) {
    FILE *file = fopen("A_FILE.HDT", "wb");
    assert(file != NULL);
    for (unsigned i = 0; i < 33; ++i) {
        uint32_t offset = i == 0 ? 0 : 2048;
        if (corrupt && i == 10) offset = 0;
        unsigned char bytes[4] = {
            (unsigned char)offset, (unsigned char)(offset >> 8),
            (unsigned char)(offset >> 16), (unsigned char)(offset >> 24)
        };
        assert(fwrite(bytes, 1, 4, file) == 4);
    }
    assert(fclose(file) == 0);
    file = fopen("A_FILE.BIN", "wb");
    assert(file != NULL);
    for (unsigned i = 0; i < 2048; ++i) assert(fputc((int)(i & 255), file) != EOF);
    assert(fclose(file) == 0);
}

int main(void) {
    GameArchive archive;
    unsigned char bytes[4] = {0};
    write_fixture(false);
    assert(game_archive_open(&archive, "."));
    assert(game_archive_read(&archive, 0, 254, bytes, 4));
    assert(bytes[0] == 254 && bytes[1] == 255 && bytes[2] == 0 && bytes[3] == 1);
    assert(!game_archive_read(&archive, 0, 2047, bytes, 4));
    assert(!game_archive_read(&archive, 32, 0, bytes, 1));
    assert(!game_archive_read(&archive, 0, UINT32_MAX, bytes, 1));
    game_archive_close(&archive);
    game_archive_close(&archive);
    write_fixture(true);
    assert(!game_archive_open(&archive, "."));
    assert(remove("A_FILE.HDT") == 0);
    assert(remove("A_FILE.BIN") == 0);
    return 0;
}
