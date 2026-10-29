#include "platform/disc_locator.h"
#include "platform/settings_json.h"
#include "platform/user_data.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HM_SETTINGS_CAPACITY 16384
#define HM_PATH_CAPACITY 4096

int hm_disc_path_json(const char *path, char *output, size_t capacity) {
    size_t length = 0;
    if (!path || !output || capacity < 3)
        return -1;
    output[length++] = '"';
    for (const unsigned char *at = (const unsigned char *)path; *at; ++at) {
        if (*at < 0x20)
            return -1;
        size_t need = (*at == '"' || *at == '\\') ? 2 : 1;
        if (length + need + 2 > capacity)
            return -1;
        if (need == 2)
            output[length++] = '\\';
        output[length++] = (char)*at;
    }
    output[length++] = '"';
    output[length] = 0;
    return (int)length;
}

int hm_disc_path_from_settings(const char *text, size_t bytes, char *path, size_t capacity) {
    const char *value;
    size_t length;
    int found = hm_json_property(text, bytes, "disc_path", &value, &length);
    if (found != 1)
        return found;
    if (length < 2 || value[0] != '"' || value[length - 1] != '"' || !capacity)
        return -1;
    size_t out = 0;
    for (size_t i = 1; i + 1 < length; ++i) {
        unsigned char c = (unsigned char)value[i];
        if (c < 0x20)
            return -1;
        if (c == '\\') {
            if (++i + 1 >= length)
                return -1;
            c = (unsigned char)value[i];
            if (c != '\\' && c != '"' && c != '/')
                return -1;
        } else if (c == '"') {
            return -1;
        }
        if (out + 1 >= capacity)
            return -1;
        path[out++] = (char)c;
    }
    path[out] = 0;
    return out ? 1 : -1;
}

static int hm_compare_names(const void *left, const void *right) {
    return SDL_strcmp(*(char *const *)left, *(char *const *)right);
}

static int hm_absolute(const char *path, char *output, size_t capacity) {
#ifdef _WIN32
    if (!_fullpath(output, path, capacity))
        return 0;
    for (char *at = output; *at; ++at)
        if (*at == '\\')
            *at = '/';
    return 1;
#else
    char *resolved = realpath(path, NULL);
    if (!resolved)
        return 0;
    int ok = strlen(resolved) < capacity;
    if (ok)
        strcpy(output, resolved);
    free(resolved);
    return ok;
#endif
}

static void hm_remember(const char *path, const char *saved) {
    char absolute[HM_PATH_CAPACITY], json[HM_PATH_CAPACITY * 2];
    if (!hm_user_data_root() || !hm_absolute(path, absolute, sizeof absolute))
        return;
    if (saved && !SDL_strcmp(saved, absolute))
        return;
    if (hm_disc_path_json(absolute, json, sizeof json) > 0 &&
        !hm_user_data_set_setting("disc_path", json))
        fprintf(stderr, "Could not remember the disc path in settings.json\n");
}

int hm_disc_locate_in(const char *argument, const char *directory, hm_disc_try try,
                      void *context) {
    char saved[HM_PATH_CAPACITY];
    int have_saved = 0;
    if (argument) {
        if (!try(context, argument, 1))
            return 0;
        /* A saved path that cannot be read is simply replaced. */
        char settings[HM_SETTINGS_CAPACITY];
        int bytes = hm_user_data_root() ? hm_user_data_read_settings(settings, sizeof settings) : -1;
        have_saved = bytes >= 0 &&
                     hm_disc_path_from_settings(settings, (size_t)bytes, saved, sizeof saved) == 1;
        hm_remember(argument, have_saved ? saved : NULL);
        return 1;
    }
    char settings[HM_SETTINGS_CAPACITY];
    int bytes = hm_user_data_root() ? hm_user_data_read_settings(settings, sizeof settings) : -1;
    if (bytes >= 0 &&
        hm_disc_path_from_settings(settings, (size_t)bytes, saved, sizeof saved) == 1) {
        have_saved = 1;
        if (try(context, saved, 0))
            return 1;
    }
    if (!directory || !*directory)
        return 0;
    int count = 0;
    char **names = SDL_GlobDirectory(directory, "*.bin", SDL_GLOB_CASEINSENSITIVE, &count);
    if (!names)
        return 0;
    qsort(names, (size_t)count, sizeof *names, hm_compare_names);
    size_t base = strlen(directory);
    while (base && (directory[base - 1] == '/' || directory[base - 1] == '\\'))
        --base;
    int found = 0;
    for (int i = 0; i < count && !found; ++i) {
        char path[HM_PATH_CAPACITY];
        if ((size_t)snprintf(path, sizeof path, "%.*s/%s", (int)base, directory, names[i]) >=
            sizeof path)
            continue;
        char absolute[HM_PATH_CAPACITY];
        if (have_saved && hm_absolute(path, absolute, sizeof absolute) &&
            !SDL_strcmp(absolute, saved))
            continue; /* already tried */
        if (try(context, path, 0)) {
            hm_remember(path, have_saved ? saved : NULL);
            found = 1;
        }
    }
    SDL_free(names);
    return found;
}

int hm_disc_locate(const char *argument, hm_disc_try try, void *context) {
    const char *env_disc = SDL_getenv("HMBTN_DISC_PATH");
    if (env_disc && *env_disc && try(context, env_disc, 1))
        return 1;
    if (hm_disc_locate_in(argument, SDL_GetBasePath(), try, context))
        return 1;
    if (hm_disc_locate_in(NULL, hm_user_data_root(), try, context))
        return 1;
    if (!argument) {
        static const char message[] =
            "Original USA disc image not found. Put the Harvest Moon: Back to Nature (USA) .bin "
            "next to the game executable, or start the game with the path to the .bin as its "
            "argument. The chosen path is remembered in Documents/VanillaBTN/settings.json.";
        fprintf(stderr, "%s\n", message);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Back to Nature", message, NULL);
    }
    return 0;
}
