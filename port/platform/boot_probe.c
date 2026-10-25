/* Diagnostic entry into compiled game C. This is not a packaged game launcher;
 * unsupported runtime calls must stop here with their named error. */
#include "psx/cd_runtime.h"
#include <limits.h>
#include <stdio.h>

extern int hm_game_main(void);

static int hm_read_at(void *context, u64 offset, void *destination, size_t bytes) {
    FILE *file = context;
    return offset <= LONG_MAX && !fseek(file, (long)offset, SEEK_SET) &&
           fread(destination, 1, bytes, file) == bytes;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: hmbtn_boot_probe <original USA BIN track>\n");
        return 1;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file || fseek(file, 0, SEEK_END)) {
        fprintf(stderr, "Cannot open the disc image\n");
        if (file)
            fclose(file);
        return 1;
    }
    long bytes = ftell(file);
    if (bytes < 0) {
        fclose(file);
        return 1;
    }
    hm_disc disc;
    hm_disc_result status = hm_disc_mount(&disc, (hm_disc_stream){file, (u64)bytes, hm_read_at});
    if (status != HM_DISC_OK) {
        fprintf(stderr, "%s\n", hm_disc_error(status));
        fclose(file);
        return 1;
    }
    hm_cd_attach(&disc);
    puts("Validated USA disc; entering compiled native game C");
    fflush(stdout);
    int result = hm_game_main();
    hm_cd_detach();
    fclose(file);
    return result;
}
