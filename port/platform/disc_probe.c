/* Local desktop bring-up tool, not the game's first-run UI. No assets are
 * written, and diagnostics never print the owner's full input path. */
#include "psx/cd_runtime.h"
#include "psx/disc.h"
#include <libcd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int hm_file_read(void *context, u64 offset, void *destination, size_t size) {
    FILE *file = context;
    return offset <= LONG_MAX && fseek(file, (long)offset, SEEK_SET) == 0 &&
           fread(destination, 1, size, file) == size;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: hmbtn_disc_probe <original USA BIN track>\n");
        return 1;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file || fseek(file, 0, SEEK_END)) {
        fprintf(stderr, "Cannot open the disc image\n");
        if (file)
            fclose(file);
        return 1;
    }
    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return 1;
    }
    hm_disc disc;
    hm_disc_result result = hm_disc_mount(&disc, (hm_disc_stream){file, (u64)length, hm_file_read});
    if (result == HM_DISC_OK) {
        hm_cd_attach(&disc);
        CdInit();
        printf("Validated USA SLUS_011.15 track: %u sectors\n", disc.sectors);
        const char *names[] = {"\\SYSTEM.CNF;1", "\\SLUS_011.15;1", "\\A_FILE.HDT;1",
                               "\\A_FILE.BIN;1"};
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
            hm_disc_entry entry;
            result = hm_disc_find(&disc, names[i], &entry);
            if (result != HM_DISC_OK)
                break;
            printf("%s: sector %u, %u bytes\n", names[i], entry.sector, entry.size);
            u8 block[2048];
            result = hm_disc_read(&disc, &entry, 0, block,
                                  entry.size < sizeof(block) ? entry.size : sizeof(block));
            if (result != HM_DISC_OK)
                break;
            CdlFILE search;
            if (!CdSearchFile(&search, names[i]) || search.size != entry.size ||
                CdPosToInt(&search.pos) != (s32)entry.sector) {
                result = HM_DISC_FORMAT;
                break;
            }
            u32 sector[512];
            CdControl(CdlSetloc, (const u8 *)&search.pos, NULL);
            CdRead(1, sector, 0x80);
            if (CdReadSync(0, NULL) ||
                memcmp(sector, block, entry.size < sizeof(block) ? entry.size : sizeof(block))) {
                result = HM_DISC_IO;
                break;
            }
        }
        if (result == HM_DISC_OK) {
            CdlFILE header, archive;
            u32 positions[512];
            if (!CdSearchFile(&header, "\\A_FILE.HDT;1") ||
                !CdSearchFile(&archive, "\\A_FILE.BIN;1") || header.size != 33 * 4) {
                result = HM_DISC_FORMAT;
            } else {
                CdControl(CdlSetloc, (const u8 *)&header.pos, NULL);
                CdRead(1, positions, 0x80);
                if (CdReadSync(0, NULL))
                    result = HM_DISC_IO;
                for (size_t i = 0; i < 33 && result == HM_DISC_OK; ++i) {
                    if (positions[i] > archive.size || (i && positions[i] < positions[i - 1]))
                        result = HM_DISC_FORMAT;
                }
                if (result == HM_DISC_OK)
                    printf("A_FILE: 32 member ranges validated against archive bounds\n");
            }
        }
        hm_cd_detach();
    }
    if (result != HM_DISC_OK)
        fprintf(stderr, "%s\n", hm_disc_error(result));
    fclose(file);
    return result != HM_DISC_OK;
}
