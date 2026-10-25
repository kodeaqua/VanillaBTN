#ifndef HMBTN_PSX_DISC_H
#define HMBTN_PSX_DISC_H

#include "compat/types.h"
#include <stddef.h>

/* The platform owns the stream, including its lifetime. Exact positional reads
 * allow desktop files and Android imported files/descriptors to share a parser. */
typedef struct {
    void *context;
    u64 size;
    int (*read_at)(void *context, u64 offset, void *destination, size_t size);
} hm_disc_stream;

typedef enum {
    HM_DISC_OK,
    HM_DISC_IO,
    HM_DISC_WRONG_IMAGE,
    HM_DISC_FORMAT,
    HM_DISC_NOT_FOUND,
    HM_DISC_RANGE,
    HM_DISC_UNSUPPORTED
} hm_disc_result;

typedef struct {
    u32 sector, size;
    u8 flags;
} hm_disc_entry;

typedef struct {
    hm_disc_stream stream;
    hm_disc_entry root;
    u32 sectors;
    int mounted;
} hm_disc;

const char *hm_disc_error(hm_disc_result result);
/* Validation is mandatory for a runtime mount. No assets are extracted. */
hm_disc_result hm_disc_mount(hm_disc *disc, hm_disc_stream stream);
hm_disc_result hm_disc_sector(hm_disc *disc, u32 lba, void *destination);
hm_disc_result hm_disc_find(hm_disc *disc, const char *path, hm_disc_entry *entry);
hm_disc_result hm_disc_read(hm_disc *disc, const hm_disc_entry *entry, u32 offset,
                            void *destination, size_t size);
/* Independent parser entry for data-free fixtures; does not mark a disc mounted. */
hm_disc_result hm_disc_decode_sector(const u8 raw[2352], u8 data[2048]);

#endif
