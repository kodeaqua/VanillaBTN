/* Synthetic SDL output tests: output follows the active aspect, including the
 * full-page clear color in the host-only sidecar. */
#include "host_display.h"
#include "sdk_gpu.h"
#include "ps1_gte.h"
#include <libgpu.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static Ps1Video video;

static int check_output(int output_width, int output_height, int expected_width) {
    SDL_Surface *surface = SDL_CreateSurface(output_width, output_height, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) return 1;
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    if (!renderer) { SDL_DestroySurface(surface); return 2; }
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, 320, 240);
    if (!texture) { SDL_DestroyRenderer(renderer); SDL_DestroySurface(surface); return 3; }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    /* Configure the actual output aspect before the game's clear. A resize
     * reallocates the sidecar; the next full-screen VRAM clear must seed it. */
    if (!host_display_update_widescreen(renderer)) return 11;
    RECT rect = {0, 240, 320, 240};
    if (ClearImage(&rect, 255, 0, 0) != 0) return 12;
    rect.x = 304; rect.w = 16;
    if (ClearImage(&rect, 0, 255, 0) != 0) return 13;
    rect.x = 272; rect.w = 16;
    if (ClearImage(&rect, 0, 0, 255) != 0) return 14;
    if (!host_present_frame(&video, renderer, &texture)) return 4;
    /* Identical output must be safe to present repeatedly without resetting
     * the logical viewport or changing the selected canvas size. */
    if (!host_present_frame(&video, renderer, &texture)) return 10;
    float tw = 0.0f, th = 0.0f;
    if (!SDL_GetTextureSize(texture, &tw, &th) || (int)tw != expected_width || th != 240.0f)
        return 5;
    uint8_t r, g, b, a;
    if (!SDL_ReadSurfacePixel(surface, 0, output_height / 2, &r, &g, &b, &a)) return 6;
    /* A PS1 full-screen clear also defines the panoramic clear color. */
    if (!(r > 200 && g < 20 && b < 20 && a == 255)) return 7;
    if (!SDL_ReadSurfacePixel(surface, output_width - 1, output_height / 2, &r, &g, &b, &a)) return 8;
    if (expected_width == 320) {
        if (!(r < 20 && g > 200 && b < 20 && a == 255)) return 9;
    } else if (!(r > 200 && g < 20 && b < 20 && a == 255)) return 9;
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    return 0;
}

int main(void) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
    sdk_gpu_bind(&video);
    DISPENV display;
    SetDefDispEnv(&display, 0, 240, 320, 240);
    if (!PutDispEnv(&display)) return 2;
    /* Each output test issues a clear after its own panoramic resize. */
    host_display_set_widescreen_mode(WIDESCREEN_ON);
    struct { int w, h, canvas; } cases[] = {
        {640,480,320}, {640,400,384}, {640,360,427},
        {840,360,560}, {1280,360,853}
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        int result = check_output(cases[i].w, cases[i].h, cases[i].canvas);
        if (result) {
            fprintf(stderr,"host panorama output %zu failed at step %d\n", i, result);
            return result;
        }
    }
    /* Stepped fades use a temporary 640x480 PS1 display mode; it must not
     * tear down the 320x240 sidecars or reset the subsequent Hor+ camera. */
    {
        SDL_Surface *fade_surface = SDL_CreateSurface(640, 360, SDL_PIXELFORMAT_ARGB8888);
        if (!fade_surface) return 16;
        SDL_Renderer *fade_renderer = SDL_CreateSoftwareRenderer(fade_surface);
        if (!fade_renderer) return 17;
        if (!host_display_update_widescreen(fade_renderer) ||
            fabsf(ps1_gte_get_widescreen_scale() - (4.0f/3.0f)) > 1e-4f)
            return 18;
        DISPENV fade_env;
        if (!SetDefDispEnv(&fade_env, 0, 0, 640, 480) || !PutDispEnv(&fade_env))
            return 19;
        if (!host_display_update_widescreen(fade_renderer) ||
            ps1_video_panorama_width(320) != 427 ||
            fabsf(ps1_gte_get_widescreen_scale() - (4.0f/3.0f)) > 1e-4f)
            return 20;
        /* No 4:3 letterbox/snap while the fullscreen fade is active:
         * reuse the previous 427x240 panoramic output and its texture. */
        int fade_w = 0, fade_h = 0;
        SDL_RendererLogicalPresentation fade_presentation;
        if (!SDL_GetRenderLogicalPresentation(fade_renderer,
                &fade_w, &fade_h, &fade_presentation) ||
            fade_w != 427 || fade_h != 240 ||
            fade_presentation != SDL_LOGICAL_PRESENTATION_STRETCH) return 22;
        SDL_Texture *fade_texture = SDL_CreateTexture(fade_renderer,
            SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);
        if (!fade_texture || !host_present_frame(&video, fade_renderer, &fade_texture))
            return 23;
        float actual_w = 0, actual_h = 0;
        if (!SDL_GetTextureSize(fade_texture, &actual_w, &actual_h) ||
            (int)actual_w != 427 || (int)actual_h != 240) return 24;
        uint8_t r = 0, g = 0, b = 0, a = 0;
        if (!SDL_ReadSurfacePixel(fade_surface, 320, 180, &r, &g, &b, &a) ||
            r < 200 || g > 20 || b > 20 || a != 255) return 25;
        SDL_DestroyTexture(fade_texture);
        if (!PutDispEnv(&display) || !host_display_update_widescreen(fade_renderer) ||
            ps1_video_panorama_width(320) != 427 ||
            fabsf(ps1_gte_get_widescreen_scale() - (4.0f/3.0f)) > 1e-4f)
            return 21;
        SDL_DestroyRenderer(fade_renderer);
        SDL_DestroySurface(fade_surface);
    }
    /* Integer scaling is an explicit user preference, and should not change
     * the dynamically calculated Hor+ canvas or default presentation mode. */
    host_display_set_integer_scaling(true);
    SDL_Surface *scaled_surface = SDL_CreateSurface(1024, 640, SDL_PIXELFORMAT_ARGB8888);
    if (!scaled_surface) return 11;
    SDL_Renderer *scaled_renderer = SDL_CreateSoftwareRenderer(scaled_surface);
    if (!scaled_renderer || !host_display_update_widescreen(scaled_renderer)) return 12;
    int logical_w = 0, logical_h = 0;
    SDL_RendererLogicalPresentation logical_mode;
    if (!SDL_GetRenderLogicalPresentation(scaled_renderer, &logical_w, &logical_h,
                                           &logical_mode)) return 13;
    if (logical_w != 384 || logical_h != 240 ||
        logical_mode != SDL_LOGICAL_PRESENTATION_INTEGER_SCALE) return 14;
    host_display_set_integer_scaling(false);
    if (!host_display_update_widescreen(scaled_renderer) ||
        !SDL_GetRenderLogicalPresentation(scaled_renderer, &logical_w, &logical_h,
                                           &logical_mode) ||
        logical_mode != SDL_LOGICAL_PRESENTATION_STRETCH) return 15;
    SDL_DestroyRenderer(scaled_renderer);
    SDL_DestroySurface(scaled_surface);
    /* CRT NTSC pixels are ~9.2% wider than square pixels. The physical
     * correction must not expand the camera or move 2D HUD coordinates. */
    {
        SDL_Surface *crt_surface = SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_ARGB8888);
        if (!crt_surface) return 26;
        SDL_Renderer *crt_renderer = SDL_CreateSoftwareRenderer(crt_surface);
        if (!crt_renderer) return 27;
        host_display_set_pixel_aspect_ratio(PAR_CRT_NTSC);
        host_display_set_widescreen_mode(WIDESCREEN_AUTO);
        if (!host_display_update_widescreen(crt_renderer) ||
            !SDL_GetRenderLogicalPresentation(crt_renderer, &logical_w, &logical_h, &logical_mode) ||
            logical_w != 349 || logical_h != 240 ||
            logical_mode != SDL_LOGICAL_PRESENTATION_LETTERBOX ||
            ps1_video_panorama_width(320) != 320 ||
            fabsf(ps1_gte_get_widescreen_scale() - 1.0f) > 1e-5f) return 28;
        SDL_Texture *crt_texture = SDL_CreateTexture(crt_renderer,
            SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 240);
        if (!crt_texture || !host_present_frame(&video, crt_renderer, &crt_texture)) return 29;
        uint8_t r = 0, g = 0, b = 0, a = 0;
        if (!SDL_ReadSurfacePixel(crt_surface, 320, 0, &r, &g, &b, &a) ||
            r != 0 || g != 0 || b != 0 || a != 255) return 30;
        SDL_DestroyTexture(crt_texture);
        SDL_DestroyRenderer(crt_renderer);
        SDL_DestroySurface(crt_surface);
    }
    {
        SDL_Surface *crt_surface = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_ARGB8888);
        if (!crt_surface) return 31;
        SDL_Renderer *crt_renderer = SDL_CreateSoftwareRenderer(crt_surface);
        if (!crt_renderer) return 32;
        host_display_set_widescreen_mode(WIDESCREEN_ON);
        float expected = (1280.0f / 720.0f) / (4.0f / 3.0f) / 1.092f;
        if (!host_display_update_widescreen(crt_renderer) ||
            fabsf(ps1_gte_get_widescreen_scale() - expected) > 1e-4f ||
            ps1_video_panorama_width(320) != 391 ||
            !SDL_GetRenderLogicalPresentation(crt_renderer, &logical_w, &logical_h, &logical_mode) ||
            logical_w != 427 || logical_h != 240 ||
            logical_mode != SDL_LOGICAL_PRESENTATION_STRETCH) return 33;
        /* The legacy stretch mode explicitly overrides CRT correction. */
        host_display_set_widescreen_mode(WIDESCREEN_STRETCH);
        if (!host_display_update_widescreen(crt_renderer) ||
            !SDL_GetRenderLogicalPresentation(crt_renderer, &logical_w, &logical_h, &logical_mode) ||
            logical_w != 320 || logical_h != 240 ||
            ps1_video_panorama_width(320) != 320) return 34;
        host_display_set_pixel_aspect_ratio(PAR_SQUARE);
        host_display_set_widescreen_mode(WIDESCREEN_AUTO);
        SDL_DestroyRenderer(crt_renderer);
        SDL_DestroySurface(crt_surface);
    }
    ps1_video_configure_panorama(0,0,1.0f);
    sdk_gpu_bind(NULL);
    SDL_Quit();
    puts("host_panorama_test: dynamic output ratios and clear-color side margins passed");
    return 0;
}
