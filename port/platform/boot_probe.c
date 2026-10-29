/* Diagnostic entry into compiled game C. This is not a packaged game launcher;
 * unsupported runtime calls must stop here with their named error. */
#include "platform/input_replay.h"
#include "compat/family_names.h"
#include "platform/crash_handler.h"
#include "overlay/game_lifecycle.h"
#include "psx/cd_runtime.h"
#include "psx/pad.h"
#include "psx/timing.h"
#ifdef HMBTN_SDL_HOST
#include "platform/disc_locator.h"
#include "platform/game_host.h"
#include "platform/user_data.h"
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

extern int hm_game_main(void);

#ifndef HMBTN_SDL_HOST
#include "farm_state.h"
#include "platform/frame_capture.h"
static hm_input_replay hm_replay;
static u64 hm_boot_tick_limit;
static int hm_boot_replay;
static int hm_boot_trace_state;
static const char *hm_boot_capture;
static void hm_boot_tick(void *context, u64 tick) {
    (void)context;
    if (hm_boot_trace_state && tick % 600 == 0) {
        const FarmPlayer *player = &game_state.player;
        fprintf(stderr,
                "Game tick %llu: area=%d place=%d,%d entry=%d player=%d,%d,%d mode=%u flags=%08x "
                "global=%08x status=%08x stamina=%d tool=%d item=%d\n",
                (unsigned long long)tick, farm_area.mode.area, farm_area.mode.place.value,
                farm_area.mode.place.location, game_state.progress.entryStep, player->position.vx,
                player->position.vy, player->position.vz, player->mode, player->stateFlags,
                game_state.globalFlags, game_state.statusFlags, player->stamina, player->held[0].id,
                player->held[1].id);
    }
    if (hm_boot_tick_limit && tick > hm_boot_tick_limit) {
        if (hm_boot_capture && !hm_frame_capture(hm_boot_capture)) {
            fprintf(stderr, "Cannot capture native diagnostic scanout\n");
            exit(1);
        }
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

typedef struct {
    FILE *file;
    hm_disc *disc;
} hm_disc_slot;

/* Open and validate one candidate BIN. On success the slot owns the open file. */
static int hm_open_disc(void *context, const char *path, int verbose) {
    hm_disc_slot *slot = context;
    FILE *file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END)) {
        if (verbose)
            fprintf(stderr, "Cannot open the disc image\n");
        if (file)
            fclose(file);
        return 0;
    }
    long bytes = ftell(file);
    if (bytes < 0) {
        fclose(file);
        return 0;
    }
    hm_disc_result status =
        hm_disc_mount(slot->disc, (hm_disc_stream){file, (u64)bytes, hm_read_at});
    if (status != HM_DISC_OK) {
        if (verbose)
            fprintf(stderr, "%s\n", hm_disc_error(status));
        fclose(file);
        return 0;
    }
    slot->file = file;
    return 1;
}

int main(int argc, char **argv) {
    hm_crash_handler_install();
    hm_disc disc;
    hm_disc_slot slot = {NULL, &disc};
#ifdef HMBTN_SDL_HOST
    if (argc > 2) {
        fprintf(stderr, "Usage: hmbtn_native [original USA BIN track]\n");
        return 1;
    }
    /* Settings hold the remembered disc path; the host reuses this running user data. */
    if (!hm_user_data_start()) {
        fprintf(stderr, "Cannot open Documents/BackToNature\n");
        return 1;
    }
    if (!hm_disc_locate(argc == 2 ? argv[1] : NULL, hm_open_disc, &slot)) {
        hm_user_data_stop();
        return 1;
    }
#else
    if (argc != 2) {
        fprintf(stderr, "Usage: hmbtn_boot_probe <original USA BIN track>\n");
        return 1;
    }
    if (!hm_open_disc(&slot, argv[1], 1))
        return 1;
#endif
    FILE *file = slot.file;
    hm_disc_result status;
    status = hm_family_names_load(&disc);
    if (status != HM_DISC_OK) {
        fprintf(stderr, "Cannot read original family text: %s\n", hm_disc_error(status));
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
    hm_boot_trace_state = getenv("HMBTN_TRACE_GAME_STATE") != NULL;
    hm_boot_capture = getenv("HMBTN_CAPTURE_FRAME");
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
    if (!hm_game_host_start(&disc)) {
        fprintf(stderr, "Cannot initialize native game window\n");
        hm_game_host_stop();
        hm_cd_detach();
        fclose(file);
        return 1;
    }
#endif
    hm_game_overlays_init();
    int result = hm_game_main();
#ifdef HMBTN_SDL_HOST
    hm_game_host_stop();
#endif
    hm_cd_detach();
    fclose(file);
    return result;
}

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    return main(__argc, __argv);
}
#endif
