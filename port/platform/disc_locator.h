#ifndef HMBTN_PLATFORM_DISC_LOCATOR_H
#define HMBTN_PLATFORM_DISC_LOCATOR_H
#include <stddef.h>

/* Called for each candidate BIN path; return 1 if the image mounted and validated.
 * `verbose` is non-zero for an explicit path, where the failure reason is reported. */
typedef int (*hm_disc_try)(void *context, const char *path, int verbose);

/* Encode `path` as a quoted JSON string value. Return length or -1. */
int hm_disc_path_json(const char *path, char *output, size_t capacity);
/* Decode the "disc_path" string from settings text. Return 1 if present, 0 if absent,
 * -1 if malformed or too long. */
int hm_disc_path_from_settings(const char *text, size_t bytes, char *path, size_t capacity);

/* Resolve the disc without UI. An explicit argument is tried alone and never replaced by a
 * fallback. Otherwise try the path saved in settings.json, then each *.bin in `directory`
 * in name order. A successful path is stored as an absolute "disc_path" setting when user
 * data is running. Return 1 on success. */
int hm_disc_locate_in(const char *argument, const char *directory, hm_disc_try try,
                      void *context);
/* Same, searching the executable's folder; reports a missing disc to the user. */
int hm_disc_locate(const char *argument, hm_disc_try try, void *context);

#endif
