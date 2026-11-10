#include "host_display.h"
#include "sdk_gpu.h"

/* Keep PS1 pixels in VRAM; SDL only presents the selected display rectangle. */
bool host_present_frame(const Ps1Video *video, SDL_Renderer *renderer, SDL_Texture **texture) {
    if (!video || !renderer || !texture || !*texture) return false;
    void *pixels;
    int pitch;
    bool copied;
    SdkDisplay display = sdk_gpu_display();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    if (!display.enabled || display.x < 0 || display.y < 0
        || display.width <= 0 || display.height <= 0
        || display.width > PS1_VRAM_WIDTH - display.x
        || display.height > PS1_VRAM_HEIGHT - display.y) {
        return SDL_RenderClear(renderer) && SDL_RenderPresent(renderer);
    }
    float width, height;
    if (!SDL_GetTextureSize(*texture, &width, &height)) return false;
    if (width != display.width || height != display.height) {
        SDL_Texture *replacement = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING, display.width, display.height);
        if (!replacement) return false;
        if (!SDL_SetTextureScaleMode(replacement, SDL_SCALEMODE_NEAREST)
            || !SDL_SetRenderLogicalPresentation(renderer, display.width, display.height,
                                                  SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
            SDL_DestroyTexture(replacement);
            return false;
        }
        SDL_DestroyTexture(*texture);
        *texture = replacement;
    }

    if (!SDL_LockTexture(*texture, NULL, &pixels, &pitch)) {
        return false;
    }
    copied = ps1_video_copy_argb(video, display.x, display.y, display.width, display.height, pixels,
                                 (size_t)pitch / sizeof(uint32_t));
    SDL_UnlockTexture(*texture);
    return copied && SDL_RenderClear(renderer)
        && SDL_RenderTexture(renderer, *texture, NULL, NULL)
        && SDL_RenderPresent(renderer);
}

