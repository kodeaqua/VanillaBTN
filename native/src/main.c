#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string.h>
#ifdef VANILLABTN_BOOT_TEST
#include <stdio.h>
#include "farm_state.h"
#include "main.h"
extern int main_scene;
#endif

#include "ps1_audio.h"
#include "ps1_card.h"
#include "ps1_input.h"
#include "ps1_video.h"
#include "sdk_gpu.h"
#include "host_display.h"
#include "sdk_pad.h"
#include "sdk_card.h"
#include "sdk_cd.h"
#include "sdk_memory.h"
#include "sdk_sound.h"
#include "sdk_sync.h"
#include <libetc.h>
#include <libcd.h>
#include <libpad.h>

_Static_assert(sizeof(void *) == 8, "The Windows port requires x64");

static Ps1Video video;
static Ps1Audio audio;
#ifndef VANILLABTN_BOOT_TEST
static Ps1CardManager card_mgr;
#endif

static void send_rumble(unsigned slot, uint8_t small, uint8_t large, void *context) {
    SDL_Gamepad *gamepad = *(SDL_Gamepad **)context;
    if (slot == 0 && gamepad != NULL) {
        /* Refresh while the SDK polls the original actuator buffer. The short
         * timeout also stops motors if host polling unexpectedly stalls. */
        SDL_RumbleGamepad(gamepad, (Uint16)(large * 257u), small ? 65535 : 0, 100);
    }
}

static uint32_t map_key_to_ps1_button(SDL_Keycode key) {
    switch (key) {
    case SDLK_UP:
    case SDLK_W:
        return PS1_PAD_UP;
    case SDLK_DOWN:
    case SDLK_S:
        return PS1_PAD_DOWN;
    case SDLK_LEFT:
    case SDLK_A:
        return PS1_PAD_LEFT;
    case SDLK_RIGHT:
    case SDLK_D:
        return PS1_PAD_RIGHT;
    case SDLK_X:
    case SDLK_K:
    case SDLK_RETURN:
        return PS1_PAD_CROSS;
    case SDLK_Z:
    case SDLK_J:
        return PS1_PAD_CIRCLE;
    case SDLK_C:
    case SDLK_U:
        return PS1_PAD_SQUARE;
    case SDLK_V:
    case SDLK_I:
        return PS1_PAD_TRIANGLE;
    case SDLK_ESCAPE:
    case SDLK_TAB:
        return PS1_PAD_SELECT;
    case SDLK_SPACE:
        return PS1_PAD_START;
    case SDLK_Q:
        return PS1_PAD_L1;
    case SDLK_E:
        return PS1_PAD_R1;
    case SDLK_1:
        return PS1_PAD_L2;
    case SDLK_2:
        return PS1_PAD_R2;
    default:
        return 0;
    }
}

static uint32_t map_gamepad_to_ps1_button(uint8_t button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:        return PS1_PAD_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return PS1_PAD_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return PS1_PAD_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return PS1_PAD_RIGHT;
    case SDL_GAMEPAD_BUTTON_SOUTH:          return PS1_PAD_CROSS;
    case SDL_GAMEPAD_BUTTON_EAST:           return PS1_PAD_CIRCLE;
    case SDL_GAMEPAD_BUTTON_WEST:           return PS1_PAD_SQUARE;
    case SDL_GAMEPAD_BUTTON_NORTH:          return PS1_PAD_TRIANGLE;
    case SDL_GAMEPAD_BUTTON_START:          return PS1_PAD_START;
    case SDL_GAMEPAD_BUTTON_BACK:           return PS1_PAD_SELECT;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return PS1_PAD_L1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return PS1_PAD_R1;
    default:                                return 0;
    }
}


typedef struct HostGame {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Gamepad *gamepad;
    bool running;
    uint32_t keyboard_buttons, controller_buttons;
    int16_t stick_x, stick_y;
#ifdef VANILLABTN_BOOT_TEST
    unsigned vblanks, vblank_limit, colored_frames, audible_frames;
    bool cycle_required, visited_farm, returned_title;
#endif
} HostGame;

#ifdef VANILLABTN_BOOT_TEST
static bool boot_save_frame(void) {
    SdkDisplay output = sdk_gpu_display();
    if (!output.enabled || output.width <= 0 || output.height <= 0 ||
        output.width > PS1_VRAM_WIDTH || output.height > PS1_VRAM_HEIGHT) return false;
    size_t count = (size_t)output.width * output.height;
    uint32_t *pixels = SDL_malloc(count * sizeof(*pixels));
    if (!pixels) return false;
    SDL_Surface *surface = NULL;
    if (ps1_video_copy_argb(&video, output.x, output.y, output.width,
        output.height, pixels, output.width)) {
        surface = SDL_CreateSurfaceFrom(output.width, output.height,
            SDL_PIXELFORMAT_ARGB8888, pixels, output.width * sizeof(*pixels));
    }
    bool saved = surface && SDL_SaveBMP(surface, "build/native-demo.bmp");
    if (surface) SDL_DestroySurface(surface);
    SDL_free(pixels);
    return saved;
}
#endif

static void host_game_pump(void *context) {
    HostGame *host = context;
    SDL_Event event;
#ifdef VANILLABTN_BOOT_TEST
    if (main_scene == SCENE_FARM && (game_state.globalFlags & GLOBAL_FLAG_INPUT_REPLAY))
        host->visited_farm = true;
    if (host->visited_farm && main_scene == SCENE_TITLE) host->returned_title = true;
    if (++host->vblanks >= host->vblank_limit || !sdk_gpu_ok()) {
        if (host->vblank_limit > 900 && host->vblanks == host->vblank_limit)
            fprintf(stderr, "frame snapshot: %d\n", boot_save_frame());
        sdk_sync_stop();
        host->running = false;
        return;
    }
#endif
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            host->running = false;
        } else if (event.type == SDL_EVENT_GAMEPAD_ADDED && host->gamepad == NULL) {
            host->gamepad = SDL_OpenGamepad(event.gdevice.which);
        } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED && host->gamepad != NULL
                   && SDL_GetGamepadID(host->gamepad) == event.gdevice.which) {
            SDL_CloseGamepad(host->gamepad);
            host->gamepad = NULL;
            host->controller_buttons = 0;
            host->stick_x = host->stick_y = 0;
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            host->keyboard_buttons = host->controller_buttons = 0;
            host->stick_x = host->stick_y = 0;
        } else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
            uint32_t btn = map_key_to_ps1_button(event.key.key);
            if (btn != 0) {
                if (event.type == SDL_EVENT_KEY_DOWN) host->keyboard_buttons |= btn;
                else host->keyboard_buttons &= ~btn;
            }
        } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                   || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
            uint32_t btn = host->gamepad != NULL && SDL_GetGamepadID(host->gamepad) == event.gbutton.which
                ? map_gamepad_to_ps1_button(event.gbutton.button) : 0;
            if (btn != 0) {
                if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) host->controller_buttons |= btn;
                else host->controller_buttons &= ~btn;
            }
        } else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && host->gamepad != NULL
                   && SDL_GetGamepadID(host->gamepad) == event.gaxis.which) {
            if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) host->stick_x = event.gaxis.value;
            if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) host->stick_y = event.gaxis.value;
        } else if (event.type == SDL_EVENT_WINDOW_EXPOSED
                   || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            if (!host_present_frame(&video, host->renderer, &host->texture)) {
                host->running = false;
                sdk_sync_stop();
                return;
            }
        }
    }

    if (!host->running) { sdk_sync_stop(); return; }
    /* Supply raw samples only: original pad.c owns the analog/repeat rules
     * in the original game. Do not feed folded stick bits. */
    sdk_pad_sample(0, true, host->gamepad != NULL,
        (uint16_t)(host->keyboard_buttons | host->controller_buttons),
        (uint8_t)(((int)host->stick_x + 32768) / 256),
        (uint8_t)(((int)host->stick_y + 32768) / 256));

    /* Original game timing owns the cadence. Submit exactly one 60 Hz audio
     * interval from the same SDK SPU used by sound.c, not the smoke mixer. */
    int16_t samples[735 * 2];
    sdk_sound_mix(samples, 735);
#ifdef VANILLABTN_BOOT_TEST
    for (unsigned i = 0; i < 735 * 2; ++i) {
        if (samples[i]) { ++host->audible_frames; break; }
    }
    SdkDisplay output = sdk_gpu_display();
    bool colored = false;
    if (output.enabled && output.x >= 0 && output.y >= 0 &&
        output.width <= PS1_VRAM_WIDTH - output.x &&
        output.height <= PS1_VRAM_HEIGHT - output.y) {
        for (int y = 0; y < output.height && !colored; ++y) {
            for (int x = 0; x < output.width; ++x) {
                unsigned pixel = video.vram[output.y + y][output.x + x] & 0x7FFF;
                if (pixel && pixel != 0x7FFF) { colored = true; break; }
            }
        }
    }
    if (colored) ++host->colored_frames;
#endif
    if (audio.stream && SDL_GetAudioStreamQueued(audio.stream) <
        (int)(sizeof(samples) * 4)) {
        SDL_PutAudioStreamData(audio.stream, samples, sizeof(samples));
    }
    if (!host_present_frame(&video, host->renderer, &host->texture)) {
        host->running = false;
        sdk_sync_stop();
    }
}

#ifdef VANILLABTN_BOOT_TEST
void boot_fault_install(void (*hook)(void));
static void boot_fault_state(void) {
    fprintf(stderr, "state: entry=%u frame=%u\n",
        game_state.progress.entryStep, game_state.frameCount);
}

static uint64_t boot_time;
static uint64_t boot_now(void *context) { (void)context; return boot_time; }
static void boot_wait(uint64_t deadline, void *context) {
    (void)context;
    boot_time = deadline;
}
#endif

int game_main(void);

int main(int argc, char **argv) {
    HostGame host = {0};
    int result = 1;
    const char *disc = "build/game-data/disc";
#ifdef VANILLABTN_BOOT_TEST
    boot_fault_install(boot_fault_state);
    host.vblank_limit = 900;
#endif
    if (argc == 2 && strcmp(argv[1], "--check-build") == 0) return 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) disc = argv[++i];
#ifdef VANILLABTN_BOOT_TEST
        else if (strcmp(argv[i], "--demo") == 0) host.vblank_limit = 5000;
        else if (strcmp(argv[i], "--cycle") == 0) {
            host.vblank_limit = 10000;
            host.cycle_required = true;
        }
#endif
        else return 1;
    }
#ifdef VANILLABTN_BOOT_TEST
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) return 1;
    if (!sdk_cd_mount(disc)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VanillaBTN",
            "Game data was not found. Import your disc and use --data with its disc directory.", NULL);
        goto done;
    }
    /* Fail before the original SDK's retry loops when required files are absent. */
    CdlFILE required;
    if (!CdSearchFile(&required, "\\A_FILE.HDT;1") ||
        !CdSearchFile(&required, "\\A_FILE.BIN;1")) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "VanillaBTN",
            "The disc directory is missing A_FILE.HDT or A_FILE.BIN.", NULL);
        goto done;
    }
    host.window = SDL_CreateWindow("VanillaBTN", 640, 480, SDL_WINDOW_RESIZABLE);
    if (!host.window) goto done;
    host.renderer = SDL_CreateRenderer(host.window, NULL);
    if (!host.renderer || !SDL_SetRenderLogicalPresentation(host.renderer,
        320, 240, SDL_LOGICAL_PRESENTATION_LETTERBOX)) goto done;
    sdk_memory_clear();
    sdk_gpu_bind(&video);
    if (!sdk_gpu_map(sdk_main_ram, SDK_RAM_SIZE)) goto done;
    host.texture = SDL_CreateTexture(host.renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 320, 240);
    if (!host.texture || !SDL_SetTextureScaleMode(host.texture, SDL_SCALEMODE_NEAREST)) goto done;
    ps1_audio_init(&audio);
#ifndef VANILLABTN_BOOT_TEST
    ps1_card_manager_init(&card_mgr, NULL);
    sdk_card_bind_manager(&card_mgr);
#else
    /* This probe must never read, recover, create or flush user save files. */
    sdk_card_bind_manager(NULL);
#endif
    sdk_pad_rumble(send_rumble, &host.gamepad);
    host.running = true;
    sdk_sync_pump(host_game_pump, &host);
#ifdef VANILLABTN_BOOT_TEST
    sdk_sync_bind(boot_now, boot_wait, NULL);
#endif
    sdk_sync_start();
    result = game_main();
#ifdef VANILLABTN_BOOT_TEST
    fprintf(stderr, "boot: vblanks=%u scene=%d gpu=%d colored=%u audible=%u\n",
        host.vblanks, main_scene, sdk_gpu_ok(), host.colored_frames, host.audible_frames);
    if (host.cycle_required) {
        fprintf(stderr, "cycle: farm=%d returned_title=%d\n",
            host.visited_farm, host.returned_title);
        if (!host.visited_farm || !host.returned_title) result = 2;
    }
    if (host.vblanks < host.vblank_limit || !sdk_gpu_ok() || !host.colored_frames) result = 2;
#endif

done:
    sdk_sync_stop();
    sdk_sync_pump(NULL, NULL);
    VSyncCallback(NULL);
    PadStopCom();
    sdk_pad_rumble(NULL, NULL);
    sdk_card_bind_manager(NULL);
    sdk_cd_unmount();
    sdk_gpu_bind(NULL);
    if (host.gamepad) SDL_CloseGamepad(host.gamepad);
#ifndef VANILLABTN_BOOT_TEST
    ps1_card_manager_shutdown(&card_mgr);
#endif
    ps1_audio_shutdown(&audio);
    if (host.texture) SDL_DestroyTexture(host.texture);
    if (host.renderer) SDL_DestroyRenderer(host.renderer);
    if (host.window) SDL_DestroyWindow(host.window);
    SDL_Quit();
    return result;
}
