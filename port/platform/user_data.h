#ifndef HMBTN_PLATFORM_USER_DATA_H
#define HMBTN_PLATFORM_USER_DATA_H
#include <stddef.h>

/* The desktop root is the user's Documents/BackToNature directory. */
int hm_user_data_start(void);
/* Explicit roots are for isolated filesystem tests, never a game-side override. */
int hm_user_data_start_at(const char *root);
void hm_user_data_stop(void);
const char *hm_user_data_root(void);
/* Return byte length or -1; preserve unknown root settings on edits. */
int hm_user_data_read_settings(char *text, size_t capacity);
int hm_user_data_set_setting(const char *key, const char *json_value);

#endif
