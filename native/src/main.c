#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string.h>

#include "ps1_video.h"

_Static_assert(sizeof(void *) == 8, "The Windows port requires x64");

static Ps1Video video;

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

    /* CTest can verify executable startup without opening a window. */
    if (argc == 2 && strcmp(argv[1], "--check-build") == 0) {
        return 0;
    }
    if (argc != 1 || !SDL_Init(SDL_INIT_VIDEO)) {
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
    while (running) {
        if (!SDL_WaitEvent(&event)) {
            goto done;
        } else if (event.type == SDL_EVENT_QUIT) {
            running = false;
        } else if ((event.type == SDL_EVENT_WINDOW_EXPOSED
                    || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
                   && !present_frame(renderer, texture)) {
            goto done;
        }
    }
    result = 0;

done:
    if (texture != NULL) SDL_DestroyTexture(texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
