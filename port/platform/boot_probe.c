/* Diagnostic entry into compiled game C. This is not a packaged game launcher;
 * unsupported runtime calls must stop here with their named error. */
#include "platform/input_replay.h"
#include "psx/cd_runtime.h"
#include "psx/pad.h"
#include "psx/timing.h"
#ifdef HMBTN_SDL_HOST
#include "platform/game_host.h"
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

extern int hm_game_main(void);

#ifndef HMBTN_SDL_HOST
static hm_input_replay hm_replay;
static u64 hm_boot_tick_limit;
static int hm_boot_replay;
static void hm_boot_tick(void *context, u64 tick) {
    (void)context;
    if (hm_boot_tick_limit && tick > hm_boot_tick_limit) {
        printf("Native diagnostic completed %llu VBlanks\n",
               (unsigned long long)hm_boot_tick_limit);
        exit(0);
    }
    if (hm_boot_replay)
        hm_pad_sample(0, 1, hm_input_replay_sample(&hm_replay, tick));
}
#endif

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
#ifndef HMBTN_SDL_HOST
    const char *replay = getenv("HMBTN_INPUT_REPLAY");
    const char *limit = getenv("HMBTN_BOOT_TICKS");
    if (replay && !hm_input_replay_load(&hm_replay, replay)) {
        fprintf(stderr, "Cannot load input replay\n");
        fclose(file);
        return 1;
    }
    hm_boot_replay = replay != NULL;
    if (limit) {
        char *end;
        hm_boot_tick_limit = strtoull(limit, &end, 10);
        if (!*limit || *end || !hm_boot_tick_limit || hm_boot_tick_limit > 1000000) {
            fprintf(stderr, "Invalid diagnostic tick limit\n");
            fclose(file);
            return 1;
        }
    }
    hm_timing_reset((hm_timing_host){NULL, hm_boot_tick, NULL});
#endif
    puts("Validated USA disc; entering compiled native game C");
    fflush(stdout);
#ifdef HMBTN_SDL_HOST
    if (!hm_game_host_start()) {
        fprintf(stderr, "Cannot initialize native game window\n");
        hm_game_host_stop();
        hm_cd_detach();
        fclose(file);
        return 1;
    }
#endif
    int result = hm_game_main();
#ifdef HMBTN_SDL_HOST
    hm_game_host_stop();
#endif
    hm_cd_detach();
    fclose(file);
    return result;
}
