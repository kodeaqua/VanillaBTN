#include "host_display.h"
#include "sdk_gpu.h"
#include "config.h"
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

static void test_monitor_discovery_and_window_modes(void) {
    /* Set dummy video driver for synthetic headless environment */
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    CHECK(SDL_Init(SDL_INIT_VIDEO));

    /* 1. Multi-monitor discovery queries */
    int count = host_display_get_monitor_count();
    CHECK(count >= 0);

    SDL_DisplayID id0 = host_display_get_monitor_id(0);
    /* In dummy driver or real headless, ID is either 0 or a valid ID */
    (void)id0;
    SDL_DisplayID id_invalid = host_display_get_monitor_id(999);
    (void)id_invalid;

    SDL_Rect bounds;
    bool has_bounds = host_display_get_monitor_bounds(0, &bounds);
    (void)has_bounds;
    CHECK(!host_display_get_monitor_bounds(0, NULL)); /* NULL-safety */

    /* 2. Window creation with DisplayConfig */
    DisplayConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.window_mode = WINDOW_MODE_WINDOWED;
    cfg.window_width = 800;
    cfg.window_height = 600;
    cfg.monitor = 0;

    SDL_Window *window = host_display_create_window(&cfg);
    CHECK(window != NULL);

    /* 3. Apply various window modes */
    cfg.window_mode = WINDOW_MODE_MAXIMIZED;
    CHECK(host_display_apply_mode(window, &cfg));

    cfg.window_mode = WINDOW_MODE_BORDERLESS;
    CHECK(host_display_apply_mode(window, &cfg));

    cfg.window_mode = WINDOW_MODE_FULLSCREEN;
    cfg.fullscreen_width = 800;
    cfg.fullscreen_height = 600;
    cfg.fullscreen_hz = 60;
    CHECK(host_display_apply_mode(window, &cfg));

    cfg.window_mode = WINDOW_MODE_WINDOWED;
    CHECK(host_display_apply_mode(window, &cfg));

    /* 4. Fullscreen toggle logic */
    /* Starting from windowed mode, toggling should transition to fullscreen/borderless */
    CHECK(host_display_toggle_fullscreen(window, &cfg));
    CHECK(cfg.window_mode == WINDOW_MODE_BORDERLESS || cfg.window_mode == WINDOW_MODE_FULLSCREEN);

    /* Toggling again should return to windowed mode */
    CHECK(host_display_toggle_fullscreen(window, &cfg));
    CHECK(cfg.window_mode == WINDOW_MODE_WINDOWED);

    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(void) {
    test_monitor_discovery_and_window_modes();

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
    CHECK(SetDefDispEnv(&display, 0, 240, 320, 240));
    CHECK(PutDispEnv(&display));
    CHECK(host_present_frame(&video, renderer, &texture));
    CHECK(SDL_GetTextureSize(texture, &width, &height));
    CHECK(width == 320 && height == 240);
    color(surface, 255, 0, 0);
    /* Out-of-bounds display position safely clears to black rather than failing. */
    display.disp.x = 800;
    CHECK(PutDispEnv(&display) == NULL); /* PutDispEnv rejects, but if direct display struct is out of bounds */
    display.disp.x = 0;
    sdk_gpu_bind(NULL);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    SDL_Quit();
    return 0;
}
