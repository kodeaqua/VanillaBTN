#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string.h>

_Static_assert(sizeof(void *) == 8, "The Windows port requires x64");

int main(int argc, char **argv) {
    SDL_Window *window;
    SDL_Event event;
    bool running = true;
    int result = 0;

    /* CTest can verify executable startup without opening a window. */
    if (argc == 2 && strcmp(argv[1], "--check-build") == 0) {
        return 0;
    }
    if (argc != 1 || !SDL_Init(SDL_INIT_VIDEO)) {
        return 1;
    }
    window = SDL_CreateWindow("VanillaBTN - native bringup", 640, 480, 0);
    if (window == NULL) {
        SDL_Quit();
        return 1;
    }
    while (running) {
        if (!SDL_WaitEvent(&event)) {
            result = 1;
            running = false;
        } else if (event.type == SDL_EVENT_QUIT) {
            running = false;
        }
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
