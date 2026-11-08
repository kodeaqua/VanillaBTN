#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string.h>

#include "ps1_audio.h"
#include "ps1_card.h"
#include "ps1_clock.h"
#include "ps1_input.h"
#include "ps1_video.h"

_Static_assert(sizeof(void *) == 8, "The Windows port requires x64");

static Ps1Video video;
static Ps1InputRecord pad1;
static Ps1Clock clock;
static Ps1Audio audio;
static Ps1CardManager card_mgr;

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

/* Keep PS1 pixels in VRAM; SDL only presents the selected display rectangle. */
static bool present_frame(SDL_Renderer *renderer, SDL_Texture *texture) {
    void *pixels;
    int pitch;
    bool copied;

    if (!SDL_LockTexture(texture, NULL, &pixels, &pitch)) {
        return false;
    }
    copied = ps1_video_copy_argb(&video, 0, 240, 320, 240, pixels,
                                 (size_t)pitch / sizeof(uint32_t));
    SDL_UnlockTexture(texture);
    return copied && SDL_RenderClear(renderer)
        && SDL_RenderTexture(renderer, texture, NULL, NULL)
        && SDL_RenderPresent(renderer);
}

int main(int argc, char **argv) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_Event event;
    bool running = true;
    int result = 1;
    SDL_Gamepad *gamepad = NULL;
    uint32_t keyboard_buttons = 0, controller_buttons = 0;
    int16_t stick_x = 0, stick_y = 0;

    /* CTest can verify executable startup without opening a window. */
    if (argc == 2 && strcmp(argv[1], "--check-build") == 0) {
        return 0;
    }
    if (argc != 1 || !SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        return 1;
    }
    window = SDL_CreateWindow("VanillaBTN - native bringup", 640, 480,
                              SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        goto done;
    }
    renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL
        || !SDL_SetRenderLogicalPresentation(renderer, 320, 240,
                                             SDL_LOGICAL_PRESENTATION_LETTERBOX)
        || !SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255)) {
        goto done;
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, 320, 240);
    if (texture == NULL || !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)
        || !present_frame(renderer, texture)) {
        goto done;
    }
    ps1_input_init(&pad1);
    ps1_clock_init(&clock, PS1_TICK_DURATION_NS, PS1_MAX_TICKS_PER_STEP);
    ps1_audio_init(&audio);
    ps1_card_manager_init(&card_mgr, NULL);
    uint64_t last_time = SDL_GetTicksNS();

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_GAMEPAD_ADDED && gamepad == NULL) {
                gamepad = SDL_OpenGamepad(event.gdevice.which);
            } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED && gamepad != NULL
                       && SDL_GetGamepadID(gamepad) == event.gdevice.which) {
                SDL_CloseGamepad(gamepad);
                gamepad = NULL;
                controller_buttons = 0;
                stick_x = stick_y = 0;
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                keyboard_buttons = controller_buttons = 0;
                stick_x = stick_y = 0;
                ps1_input_clear(&pad1);
            } else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                uint32_t btn = map_key_to_ps1_button(event.key.key);
                if (btn != 0) {
                    if (event.type == SDL_EVENT_KEY_DOWN) keyboard_buttons |= btn;
                    else keyboard_buttons &= ~btn;
                }
            } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
                       || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
                uint32_t btn = gamepad != NULL && SDL_GetGamepadID(gamepad) == event.gbutton.which
                    ? map_gamepad_to_ps1_button(event.gbutton.button) : 0;
                if (btn != 0) {
                    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) controller_buttons |= btn;
                    else controller_buttons &= ~btn;
                }
            } else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && gamepad != NULL
                       && SDL_GetGamepadID(gamepad) == event.gaxis.which) {
                if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) stick_x = event.gaxis.value;
                if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) stick_y = event.gaxis.value;
            } else if (event.type == SDL_EVENT_WINDOW_EXPOSED
                       || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                if (!present_frame(renderer, texture)) {
                    goto done;
                }
            }
        }

        if (!running) break;
        pad1.held = keyboard_buttons | controller_buttons;
        if ((controller_buttons & PS1_PAD_ARROWS_MASK) == 0) {
            pad1.held |= ps1_input_stick_buttons(stick_x, stick_y);
        }

        uint64_t now = SDL_GetTicksNS();
        uint32_t ticks = ps1_clock_advance(&clock, now - last_time);
        last_time = now;

        while (ticks-- > 0) {
            ps1_input_tick(&pad1);
            ps1_audio_tick(&audio, 1);
        }

        if (!present_frame(renderer, texture)) {
            goto done;
        }

        SDL_Delay(1);
    }
    result = 0;

done:
    if (gamepad != NULL) SDL_CloseGamepad(gamepad);
    ps1_card_manager_shutdown(&card_mgr);
    ps1_audio_shutdown(&audio);
    if (texture != NULL) SDL_DestroyTexture(texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
