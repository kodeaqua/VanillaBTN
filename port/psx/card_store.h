#ifndef HMBTN_PSX_CARD_STORE_H
#define HMBTN_PSX_CARD_STORE_H
#include <libmcrd.h>
s32 hm_card_status(s32 port);
s32 hm_card_format(s32 port, int formatted);
s32 hm_card_create(s32 port, const char *name, s32 blocks);
s32 hm_card_directory(s32 port, const char *pattern, struct DIRENTRY *entries, s32 *count,
                      s32 offset, s32 maximum);
s32 hm_card_transfer(s32 port, const char *name, void *data, s32 offset, s32 bytes, int write);
#endif
