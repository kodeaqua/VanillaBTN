#ifndef HMBTN_PLATFORM_USER_DATA_H
#define HMBTN_PLATFORM_USER_DATA_H

/* The desktop root is the user's Documents/BackToNature directory. */
int hm_user_data_start(void);
/* Explicit roots are for isolated filesystem tests, never a game-side override. */
int hm_user_data_start_at(const char *root);
void hm_user_data_stop(void);
const char *hm_user_data_root(void);

#endif
