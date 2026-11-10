#ifndef VANILLABTN_GAME_ARCHIVE_H
#define VANILLABTN_GAME_ARCHIVE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct GameArchive {
    FILE *file;
    uint32_t offsets[33];
} GameArchive;

/* Development importer output: A_FILE.HDT contains byte offsets, not sectors. */
bool game_archive_open(GameArchive *archive, const char *directory);
bool game_archive_read(GameArchive *archive, unsigned member, uint32_t offset,
                       void *destination, uint32_t size);
void game_archive_close(GameArchive *archive);

#endif
