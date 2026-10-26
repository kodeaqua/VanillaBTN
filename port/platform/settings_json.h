#ifndef HMBTN_PLATFORM_SETTINGS_JSON_H
#define HMBTN_PLATFORM_SETTINGS_JSON_H
#include <stddef.h>
/* Validate an object and locate a literal ASCII property. Return 1 if present,
 * 0 if absent, -1 if malformed or ambiguous. Spans refer to the input bytes. */
int hm_json_property(const char *text, size_t bytes, const char *key, const char **value,
                     size_t *length);
int hm_json_integer(const char *text, size_t bytes, const char *key, int *value);
int hm_json_replace(const char *text, size_t bytes, const char *key, const char *replacement,
                    char *output, size_t capacity);
#endif
