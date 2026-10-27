#ifndef HMBTN_SAVE_CODEC_H
#define HMBTN_SAVE_CODEC_H
#include "compat/types.h"
#include <stddef.h>
struct FarmState;
enum { HM_SAVE_IMAGE_BYTES = 0x6378 };
/* Original unshifted PS1 payload. Native runtime addresses never go on disk. */
int hm_save_encode(void *image, size_t bytes, struct FarmState *state);
int hm_save_decode(const void *image, size_t bytes, struct FarmState *state);
#endif
