#include "host_renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "Assertion failed at line %d: %s\n", __LINE__, #cond); \
    exit(1); \
} } while (0)

static void test_driver_queries(void) {
    int count = host_renderer_get_driver_count();
    CHECK(count >= 0);

    /* NULL check */
    CHECK(!host_renderer_is_driver_available(NULL));
    CHECK(!host_renderer_is_driver_available("non_existent_gpu_driver_xyz"));

    /* Boundary checks */
    CHECK(host_renderer_get_driver_name(-1) == NULL);
    CHECK(host_renderer_get_driver_name(9999) == NULL);

    for (int i = 0; i < count; ++i) {
        const char *name = host_renderer_get_driver_name(i);
        CHECK(name != NULL);
        CHECK(strlen(name) > 0);
        CHECK(host_renderer_is_driver_available(name));
    }
}

static void test_backend_name_mapping(void) {
    CHECK(host_renderer_driver_for_backend(BACKEND_AUTO) == NULL);
    CHECK(strcmp(host_renderer_driver_for_backend(BACKEND_VULKAN), "vulkan") == 0);
    CHECK(strcmp(host_renderer_driver_for_backend(BACKEND_D3D11), "direct3d11") == 0);
    CHECK(strcmp(host_renderer_driver_for_backend(BACKEND_D3D12), "direct3d12") == 0);
    CHECK(strcmp(host_renderer_driver_for_backend(BACKEND_OPENGL), "opengl") == 0);
    CHECK(strcmp(host_renderer_driver_for_backend(BACKEND_SOFTWARE), "software") == 0);
}

static void test_renderer_fallback_creation(void) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    CHECK(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window *window = SDL_CreateWindow("TestRenderer", 320, 240, 0);
    CHECK(window != NULL);

    /* NULL window safety */
    CHECK(host_renderer_create(NULL, BACKEND_AUTO, false) == NULL);

    /* 1. Request Auto backend */
    SDL_Renderer *r_auto = host_renderer_create(window, BACKEND_AUTO, true);
    CHECK(r_auto != NULL);
    const char *name_auto = host_renderer_get_active_name(r_auto);
    CHECK(name_auto != NULL);
    SDL_DestroyRenderer(r_auto);

    /* 2. Request Software backend */
    SDL_Renderer *r_sw = host_renderer_create(window, BACKEND_SOFTWARE, false);
    CHECK(r_sw != NULL);
    const char *name_sw = host_renderer_get_active_name(r_sw);
    CHECK(name_sw != NULL);
    CHECK(SDL_strcasecmp(name_sw, "software") == 0);
    SDL_DestroyRenderer(r_sw);

    /* 3. Request specific backend that triggers fallback chain if unavailable */
    SDL_Renderer *r_vulkan = host_renderer_create(window, BACKEND_VULKAN, true);
    CHECK(r_vulkan != NULL); /* Must succeed via fallback chain even in dummy video */
    const char *name_vk = host_renderer_get_active_name(r_vulkan);
    CHECK(name_vk != NULL);
    SDL_DestroyRenderer(r_vulkan);

    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(void) {
    test_driver_queries();
    test_backend_name_mapping();
    test_renderer_fallback_creation();

    printf("host_renderer_test: All multi-backend graphics abstraction tests passed successfully.\n");
    return 0;
}
