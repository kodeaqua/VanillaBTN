#include "platform/user_data.h"
#include <SDL3/SDL.h>
#include <libmcrd.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv) {
    CHECK(argc == 2);
    char *root = NULL, *settings = NULL, *path = NULL;
    /* Isolated unique roots: no access to the owner's real Documents or saves. */
    CHECK(SDL_asprintf(&root, "%s-%llu-\xc3\xa9", argv[1],
                       (unsigned long long)SDL_GetPerformanceCounter()) >= 0);
    CHECK(hm_user_data_start_at(root));
    CHECK(!strcmp(hm_user_data_root(), root));
    CHECK(!hm_user_data_start_at(root));
    CHECK(SDL_asprintf(&settings, "%s/settings.json", root) >= 0);
    SDL_IOStream *file = SDL_IOFromFile(settings, "rb");
    CHECK(file);
    char initial[64] = {0};
    CHECK(SDL_ReadIO(file, initial, sizeof(initial) - 1) > 0 && SDL_CloseIO(file));
    CHECK(strstr(initial, "\"schema_version\": 1"));
    MemCardInit(0);
    MemCardStart();
    CHECK(MemCardCreateFile(0, "SAVE01", 4) == 0);
    const unsigned char expected[] = {0, 255, 42, 71, 0, 123};
    CHECK(MemCardWriteFile(0, "SAVE01", expected, 128, sizeof(expected)) == 1);
    s32 command, result;
    CHECK(MemCardSync(0, &command, &result) == 1 && command == 4 && result == 0);
    hm_user_data_stop();
    /* Preserve existing settings, even keys unknown to this build. */
    file = SDL_IOFromFile(settings, "wb");
    static const char custom[] = "{\"owner_setting\":42}\n";
    CHECK(file && SDL_WriteIO(file, custom, sizeof(custom) - 1) == sizeof(custom) - 1);
    CHECK(SDL_CloseIO(file));
    CHECK(hm_user_data_start_at(root));
    unsigned char actual[sizeof(expected)] = {0};
    MemCardInit(0);
    MemCardStart();
    CHECK(MemCardReadFile(0, "SAVE01", actual, 128, sizeof(actual)) == 1);
    CHECK(MemCardSync(0, &command, &result) == 1 && result == 0);
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    /* Deny creation of the sibling temporary file. The old disk data and
     * committed in-memory data must both survive the failed save. */
    CHECK(SDL_asprintf(&path, "%s/saves/card1.hmc.tmp", root) >= 0);
    CHECK(SDL_CreateDirectory(path));
    const unsigned char changed[] = {1, 2, 3, 4, 5, 6};
    CHECK(MemCardWriteFile(0, "SAVE01", changed, 128, sizeof(changed)) == 1);
    CHECK(MemCardSync(0, &command, &result) == 1 && result == McErrCardInvalid);
    CHECK(MemCardReadFile(0, "SAVE01", actual, 128, sizeof(actual)) == 1);
    CHECK(MemCardSync(0, &command, &result) == 1 && result == 0);
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    CHECK(SDL_RemovePath(path));
    SDL_free(path);
    hm_user_data_stop();
    CHECK(hm_user_data_start_at(root));
    MemCardInit(0);
    MemCardStart();
    CHECK(MemCardReadFile(0, "SAVE01", actual, 128, sizeof(actual)) == 1);
    CHECK(MemCardSync(0, &command, &result) == 1 && result == 0);
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    file = SDL_IOFromFile(settings, "rb");
    memset(initial, 0, sizeof(initial));
    CHECK(file && SDL_ReadIO(file, initial, sizeof(initial) - 1) == sizeof(custom) - 1);
    CHECK(SDL_CloseIO(file) && !strcmp(initial, custom));
    CHECK(hm_user_data_set_setting("graphics", "{\"width\":1920}"));
    char updated[256];
    CHECK(hm_user_data_read_settings(updated, sizeof(updated)) > 0);
    CHECK(strstr(updated, "\"owner_setting\":42") && strstr(updated, "\"width\":1920"));
    hm_user_data_stop();
    CHECK(SDL_RemovePath(settings));
    const char *names[] = {"saves/card1.hmc", "saves/card2.hmc", "saves", ".lock"};
    for (size_t i = 0; i < SDL_arraysize(names); ++i) {
        CHECK(SDL_asprintf(&path, "%s/%s", root, names[i]) >= 0);
        if (i < 3)
            CHECK(SDL_RemovePath(path));
        else
            SDL_RemovePath(path);
        SDL_free(path);
    }
    CHECK(SDL_RemovePath(root));
    SDL_free(settings);
    SDL_free(root);
    return 0;
}
