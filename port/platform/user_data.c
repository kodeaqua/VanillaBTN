#include "platform/user_data.h"
#include "platform/settings_json.h"
#include "psx/card.h"
#include <SDL3/SDL.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE hm_data_lock = INVALID_HANDLE_VALUE;
#endif

static char *hm_root;
static char *hm_saves;

typedef struct {
    const char *name;
    int found;
} hm_lookup;
static SDL_EnumerationResult hm_find(void *context, const char *directory, const char *name) {
    (void)directory;
    hm_lookup *lookup = context;
    if (!SDL_strcmp(name, lookup->name))
        lookup->found = 1;
    return SDL_ENUM_CONTINUE;
}
static int hm_exists(const char *directory, const char *name) {
    hm_lookup lookup = {name, 0};
    return SDL_EnumerateDirectory(directory, hm_find, &lookup) ? lookup.found : -1;
}
static int hm_replace(const char *directory, const char *name, const void *data, size_t bytes) {
    char *path = NULL, *temporary = NULL;
    if (SDL_asprintf(&path, "%s/%s", directory, name) < 0 ||
        SDL_asprintf(&temporary, "%s/%s.tmp", directory, name) < 0) {
        SDL_free(path);
        SDL_free(temporary);
        return -1;
    }
    SDL_IOStream *file = SDL_IOFromFile(temporary, "wb");
    bool ok = file && SDL_WriteIO(file, data, bytes) == bytes && SDL_FlushIO(file);
    if (file && !SDL_CloseIO(file))
        ok = false;
    if (ok)
        ok = SDL_RenamePath(temporary, path);
    if (!ok && file)
        SDL_RemovePath(temporary);
    SDL_free(temporary);
    SDL_free(path);
    return ok ? 1 : -1;
}
static const char *hm_card_name(int slot) { return slot ? "card2.hmc" : "card1.hmc"; }
static int hm_read_card(void *context, int slot, void *data, size_t bytes) {
    const char *directory = context;
    const char *name = hm_card_name(slot);
    int exists = hm_exists(directory, name);
    if (exists != 1)
        return exists;
    char *path = NULL;
    if (SDL_asprintf(&path, "%s/%s", directory, name) < 0)
        return -1;
    SDL_IOStream *file = SDL_IOFromFile(path, "rb");
    SDL_free(path);
    if (!file)
        return -1;
    bool ok = SDL_GetIOSize(file) == (Sint64)bytes && SDL_ReadIO(file, data, bytes) == bytes;
    if (!SDL_CloseIO(file))
        ok = false;
    return ok ? 1 : -1;
}
static int hm_write_card(void *context, int slot, const void *data, size_t bytes) {
    return hm_replace(context, hm_card_name(slot), data, bytes);
}
const char *hm_user_data_root(void) { return hm_root; }
void hm_user_data_stop(void) {
    hm_card_detach();
#ifdef _WIN32
    if (hm_data_lock != INVALID_HANDLE_VALUE)
        CloseHandle(hm_data_lock);
    hm_data_lock = INVALID_HANDLE_VALUE;
#endif
    SDL_free(hm_saves);
    SDL_free(hm_root);
    hm_saves = hm_root = NULL;
}
int hm_user_data_start_at(const char *root) {
    if (hm_root || !root || !*root)
        return 0;
    hm_root = SDL_strdup(root);
    if (!hm_root || SDL_asprintf(&hm_saves, "%s/saves", root) < 0 ||
        !SDL_CreateDirectory(hm_root) || !SDL_CreateDirectory(hm_saves))
        goto fail;
#ifdef _WIN32
    /* Exclude a second process before it can read stale card state or replace
     * another writer's temporary file. The lock file contains no user data. */
    char *lock_path = NULL;
    if (SDL_asprintf(&lock_path, "%s/.lock", hm_root) < 0)
        goto fail;
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, lock_path, -1, NULL, 0);
    wchar_t *wide = count ? SDL_malloc((size_t)count * sizeof(wchar_t)) : NULL;
    if (wide && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, lock_path, -1, wide, count))
        hm_data_lock = CreateFileW(wide, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, NULL);
    SDL_free(wide);
    SDL_free(lock_path);
    if (hm_data_lock == INVALID_HANDLE_VALUE)
        goto fail;
#endif
    int settings = hm_exists(hm_root, "settings.json");
    if (settings < 0)
        goto fail;
    if (!settings) {
        static const char initial[] = "{\n  \"schema_version\": 1\n}\n";
        if (hm_replace(hm_root, "settings.json", initial, sizeof(initial) - 1) != 1)
            goto fail;
    }
    hm_card_attach((hm_card_storage){hm_saves, hm_read_card, hm_write_card});
    return 1;
fail:
    hm_user_data_stop();
    return 0;
}
int hm_user_data_start(void) {
    const char *documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    char *root = NULL;
    if (!documents || SDL_asprintf(&root, "%s/BackToNature", documents) < 0)
        return 0;
    int ok = hm_user_data_start_at(root);
    SDL_free(root);
    return ok;
}
int hm_user_data_read_settings(char *text, size_t capacity) {
    if (!hm_root || !text || !capacity)
        return -1;
    char *path = NULL;
    if (SDL_asprintf(&path, "%s/settings.json", hm_root) < 0)
        return -1;
    size_t bytes = 0;
    void *data = SDL_LoadFile(path, &bytes);
    SDL_free(path);
    if (!data || bytes >= capacity || bytes > 65536) {
        SDL_free(data);
        return -1;
    }
    SDL_memcpy(text, data, bytes);
    text[bytes] = 0;
    SDL_free(data);
    return (int)bytes;
}
int hm_user_data_set_nested_setting(const char *parent, const char *key, const char *json_value) {
    char *text = SDL_malloc(65537), *nested = SDL_malloc(66561);
    int bytes = text && nested ? hm_user_data_read_settings(text, 65537) : -1;
    const char *object = "{}";
    size_t length = 2;
    int found = bytes >= 0 ? hm_json_property(text, (size_t)bytes, parent, &object, &length) : -1;
    int ok = found >= 0 && hm_json_replace(object, length, key, json_value, nested, 66561);
    if (ok)
        ok = hm_user_data_set_setting(parent, nested);
    SDL_free(nested);
    SDL_free(text);
    return ok;
}
int hm_user_data_set_setting(const char *key, const char *json_value) {
    char *text = SDL_malloc(65537), *output = SDL_malloc(66561);
    int bytes = text && output ? hm_user_data_read_settings(text, 65537) : -1;
    int ok = bytes >= 0 && hm_json_replace(text, (size_t)bytes, key, json_value, output, 66561);
    if (ok) {
        size_t length = SDL_strlen(output);
        ok = length <= 65536 && hm_replace(hm_root, "settings.json", output, length) == 1;
    }
    SDL_free(output);
    SDL_free(text);
    return ok;
}
