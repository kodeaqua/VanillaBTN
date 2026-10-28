#include "platform/disc_locator.h"
#include "platform/user_data.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct {
    char tried[8][512];
    int count;
    const char *accept_suffix;
} probe;

static int hm_probe(void *context, const char *path, int verbose) {
    probe *state = context;
    (void)verbose;
    if (state->count < 8)
        snprintf(state->tried[state->count], sizeof state->tried[0], "%s", path);
    state->count++;
    size_t length = strlen(path), suffix = strlen(state->accept_suffix);
    return length >= suffix && !strcmp(path + length - suffix, state->accept_suffix);
}

static int touch(const char *directory, const char *name) {
    char *path = NULL;
    if (SDL_asprintf(&path, "%s/%s", directory, name) < 0)
        return 0;
    SDL_IOStream *file = SDL_IOFromFile(path, "wb");
    SDL_free(path);
    return file && SDL_CloseIO(file);
}

static int saved_matches(const char *suffix) {
    char settings[4096], path[1024];
    int bytes = hm_user_data_read_settings(settings, sizeof settings);
    if (bytes < 0 || hm_disc_path_from_settings(settings, (size_t)bytes, path, sizeof path) != 1)
        return 0;
    size_t length = strlen(path), tail = strlen(suffix);
    return length >= tail && !strcmp(path + length - tail, suffix);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    char json[256], decoded[256];
    const char *awkward = "C:\\Games\\\"Back\" to/Nature \xc3\xa9.bin";
    int length = hm_disc_path_json(awkward, json, sizeof json);
    CHECK(length > 0);
    char settings[512];
    int bytes = snprintf(settings, sizeof settings, "{\"schema_version\":1,\"disc_path\":%s}", json);
    CHECK(hm_disc_path_from_settings(settings, (size_t)bytes, decoded, sizeof decoded) == 1);
    CHECK(!strcmp(decoded, awkward));
    CHECK(hm_disc_path_json("a\nb", json, sizeof json) == -1);
    CHECK(hm_disc_path_json("0123456789", json, 8) == -1);
    static const char absent[] = "{\"schema_version\":1}";
    CHECK(hm_disc_path_from_settings(absent, sizeof absent - 1, decoded, sizeof decoded) == 0);
    static const char bad[] = "{\"disc_path\":\"a\\q\"}";
    CHECK(hm_disc_path_from_settings(bad, sizeof bad - 1, decoded, sizeof decoded) == -1);
    static const char number[] = "{\"disc_path\":5}";
    CHECK(hm_disc_path_from_settings(number, sizeof number - 1, decoded, sizeof decoded) == -1);
    static const char empty[] = "{\"disc_path\":\"\"}";
    CHECK(hm_disc_path_from_settings(empty, sizeof empty - 1, decoded, sizeof decoded) == -1);

    /* Isolated unique root: no access to the owner's real Documents or settings. */
    char *root = NULL, *discs = NULL;
    CHECK(SDL_asprintf(&root, "%s-%llu", argv[1], (unsigned long long)SDL_GetPerformanceCounter()) >=
          0);
    CHECK(SDL_asprintf(&discs, "%s/discs", root) >= 0);
    CHECK(hm_user_data_start_at(root));
    CHECK(SDL_CreateDirectory(discs));
    CHECK(touch(discs, "a.BIN") && touch(discs, "b.bin") && touch(discs, "c.txt"));

    probe state = {.accept_suffix = "b.bin"};
    CHECK(hm_disc_locate_in(NULL, discs, hm_probe, &state) == 1);
    CHECK(state.count == 2); /* a.BIN rejected, then b.bin; c.txt never tried */
    CHECK(strstr(state.tried[0], "a.BIN") && strstr(state.tried[1], "b.bin"));
    CHECK(saved_matches("b.bin"));

    /* The saved path is tried first and the folder is not scanned when it works. */
    state = (probe){.accept_suffix = "b.bin"};
    CHECK(hm_disc_locate_in(NULL, NULL, hm_probe, &state) == 1);
    CHECK(state.count == 1 && strstr(state.tried[0], "b.bin"));

    /* An explicit argument is never replaced by a fallback, and failure saves nothing new. */
    state = (probe){.accept_suffix = "nothing"};
    CHECK(hm_disc_locate_in("missing.bin", discs, hm_probe, &state) == 0);
    CHECK(state.count == 1 && saved_matches("b.bin"));

    /* A working explicit argument is remembered. */
    char *explicit_path = NULL;
    CHECK(SDL_asprintf(&explicit_path, "%s/a.BIN", discs) >= 0);
    state = (probe){.accept_suffix = "a.BIN"};
    CHECK(hm_disc_locate_in(explicit_path, discs, hm_probe, &state) == 1);
    CHECK(state.count == 1 && saved_matches("a.BIN"));

    /* Nothing usable anywhere reports failure. */
    state = (probe){.accept_suffix = "nothing"};
    CHECK(hm_disc_locate_in(NULL, discs, hm_probe, &state) == 0);

    SDL_free(explicit_path);
    hm_user_data_stop();
    SDL_free(discs);
    SDL_free(root);
    return 0;
}
