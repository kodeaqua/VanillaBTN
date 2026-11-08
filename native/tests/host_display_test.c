#include "host_display.h"
#include "sdk_gpu.h"
#include <libgpu.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s (%s)\n", __LINE__, #condition, SDL_GetError()); \
    exit(1); \
} } while (0)

static Ps1Video video;

static void color(SDL_Surface *surface, unsigned char r, unsigned char g, unsigned char b) {
    unsigned char actual_r, actual_g, actual_b, actual_a;
    CHECK(SDL_ReadSurfacePixel(surface, 10, 10, &actual_r, &actual_g, &actual_b, &actual_a));
    CHECK(actual_r == r && actual_g == g && actual_b == b && actual_a == 255);
}

int main(void) {
    CHECK(SDL_Init(0));
    /* Render into a memory surface: no physical window, device or gameplay. */
    SDL_Surface *surface = SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_ARGB8888);
    CHECK(surface != NULL);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    CHECK(renderer != NULL);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, 320, 240);
    CHECK(texture != NULL);
    CHECK(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255));
    CHECK(SDL_SetRenderLogicalPresentation(renderer, 320, 240, SDL_LOGICAL_PRESENTATION_LETTERBOX));
    CHECK(SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST));
    sdk_gpu_bind(&video);
    RECT top = {0, 0, 320, 240}, bottom = {0, 240, 320, 240};
    CHECK(ClearImage(&top, 0, 0, 255) == 0);
    CHECK(ClearImage(&bottom, 255, 0, 0) == 0);
    CHECK(host_present_frame(&video, renderer, &texture));
    color(surface, 255, 0, 0);
    DISPENV display;
    CHECK(SetDefDispEnv(&display, 0, 0, 320, 240));
    CHECK(PutDispEnv(&display));
    CHECK(host_present_frame(&video, renderer, &texture));
    color(surface, 0, 0, 255);
    SetDispMask(0);
    CHECK(host_present_frame(&video, renderer, &texture));
    color(surface, 0, 0, 0);
    SetDispMask(1);
    CHECK(SetDefDispEnv(&display, 0, 0, 640, 480));
    CHECK(PutDispEnv(&display));
    CHECK(host_present_frame(&video, renderer, &texture));
    float width, height;
    CHECK(SDL_GetTextureSize(texture, &width, &height));
    CHECK(width == 640 && height == 480);
    color(surface, 0, 0, 255);
    sdk_gpu_bind(NULL);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    SDL_Quit();
    return 0;
}
