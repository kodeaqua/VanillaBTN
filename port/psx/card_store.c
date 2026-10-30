#include "psx/card_store.h"
#include "psx/card.h"
#include <string.h>

/* Native container v1: 32-byte header, fifteen 32-byte file records, and
 * fifteen 8192-byte data blocks. All disk integers are explicit little endian.
 * This is an application format, not a raw PS1 memory-card image. */
enum {
    HM_BLOCK = 8192,
    HM_BLOCKS = 15,
    HM_HEADER = 512,
    HM_CARD_BYTES = HM_HEADER + HM_BLOCK * HM_BLOCKS
};
static hm_card_storage hm_storage;
static unsigned char hm_images[2][HM_CARD_BYTES], hm_candidate[HM_CARD_BYTES];
static s32 hm_status[2] = {McErrCardNotExist, McErrCardNotExist};
static const unsigned char hm_magic[8] = {'H', 'M', 'B', 'T', 'N', 'C', 1, 0};

static unsigned hm_get(const unsigned char *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}
static void hm_put(unsigned char *p, unsigned n) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (unsigned char)(n >> (i * 8));
}
static int hm_slot(s32 port) { return port == 0 ? 0 : port == 0x10 ? 1 : -1; }
static int hm_name(const char *name) {
    if (!name || !*name)
        return 0;
    size_t n = 0;
    for (; name[n]; ++n) {
        unsigned char c = (unsigned char)name[n];
        if (n >= 20 || c < 33 || c > 126 || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?')
            return 0;
    }
    return strcmp(name, ".") && strcmp(name, "..");
}
static unsigned char *hm_entry(unsigned char *image, int i) { return image + 32 + i * 32; }
static void hm_empty(unsigned char *image, int formatted) {
    memset(image, 0, HM_CARD_BYTES);
    memcpy(image, hm_magic, sizeof(hm_magic));
    image[8] = (unsigned char)formatted;
}
static int hm_valid(unsigned char *image) {
    if (memcmp(image, hm_magic, sizeof(hm_magic)) || image[8] > 1)
        return 0;
    unsigned used = 0;
    for (int i = 0; i < HM_BLOCKS; ++i) {
        unsigned char *e = hm_entry(image, i);
        if (!e[0])
            continue;
        if (!memchr(e, 0, 21) || !hm_name((const char *)e))
            return 0;
        unsigned blocks = hm_get(e + 24), start = hm_get(e + 28);
        if (!blocks || blocks > HM_BLOCKS || start > HM_BLOCKS - blocks)
            return 0;
        unsigned mask = ((1u << blocks) - 1u) << start;
        if (used & mask)
            return 0;
        used |= mask;
        for (int j = 0; j < i; ++j)
            if (!strcmp((char *)e, (char *)hm_entry(image, j)))
                return 0;
    }
    return 1;
}
static s32 hm_commit(int slot) {
    if (hm_storage.write(hm_storage.context, slot, hm_candidate, HM_CARD_BYTES) != 1)
        return McErrCardInvalid;
    memcpy(hm_images[slot], hm_candidate, HM_CARD_BYTES);
    hm_status[slot] = hm_candidate[8] ? McErrNone : McErrNotFormat;
    return McErrNone;
}
void hm_card_detach(void) {
    memset(&hm_storage, 0, sizeof(hm_storage));
    hm_status[0] = hm_status[1] = McErrCardNotExist;
}
void hm_card_attach(hm_card_storage storage) {
    hm_card_detach();
    if (!storage.read || !storage.write)
        return;
    hm_storage = storage;
    for (int slot = 0; slot < 2; ++slot) {
        int result = storage.read(storage.context, slot, hm_images[slot], HM_CARD_BYTES);
        if (result == 0) {
            hm_empty(hm_candidate, 1);
            hm_status[slot] = hm_commit(slot);
        } else if (result != 1 || !hm_valid(hm_images[slot])) {
            hm_status[slot] = McErrCardInvalid;
        } else {
            hm_status[slot] = hm_images[slot][8] ? McErrNone : McErrNotFormat;
        }
    }
}
s32 hm_card_status(s32 port) {
    int slot = hm_slot(port);
    return slot < 0 ? McErrCardNotExist : hm_status[slot];
}
s32 hm_card_format(s32 port, int formatted) {
    int slot = hm_slot(port);
    if (slot < 0 || !hm_storage.write)
        return McErrCardNotExist;
    hm_empty(hm_candidate, formatted);
    return hm_commit(slot);
}
static int hm_find(int slot, const char *name) {
    for (int i = 0; i < HM_BLOCKS; ++i)
        if (!strcmp((char *)hm_entry(hm_images[slot], i), name))
            return i;
    return -1;
}
s32 hm_card_create(s32 port, const char *name, s32 blocks) {
    s32 status = hm_card_status(port);
    if (status)
        return status;
    if (!hm_name(name) || blocks <= 0 || blocks > HM_BLOCKS)
        return McErrCardInvalid;
    int slot = hm_slot(port), free_entry = -1;
    if (hm_find(slot, name) >= 0)
        return McErrAlreadyExist;
    unsigned used = 0;
    for (int i = 0; i < HM_BLOCKS; ++i) {
        unsigned char *e = hm_entry(hm_images[slot], i);
        if (!e[0]) {
            if (free_entry < 0)
                free_entry = i;
            continue;
        }
        used |= ((1u << hm_get(e + 24)) - 1u) << hm_get(e + 28);
    }
    if (free_entry < 0)
        return McErrBlockFull;
    for (int start = 0; start <= HM_BLOCKS - blocks; ++start) {
        unsigned mask = ((1u << blocks) - 1u) << start;
        if (used & mask)
            continue;
        memcpy(hm_candidate, hm_images[slot], HM_CARD_BYTES);
        unsigned char *e = hm_entry(hm_candidate, free_entry);
        memset(e, 0, 32);
        memcpy(e, name, strlen(name));
        hm_put(e + 24, (unsigned)blocks);
        hm_put(e + 28, (unsigned)start);
        memset(hm_candidate + HM_HEADER + start * HM_BLOCK, 0, blocks * HM_BLOCK);
        return hm_commit(slot);
    }
    return McErrBlockFull;
}
static int hm_matches(const char *pattern, const char *name) {
    /* The game requests exact names or '*'. Support the usual wildcard subset. */
    const char *star = NULL, *retry = NULL;
    while (*name) {
        if (*pattern == '?' || *pattern == *name) {
            ++pattern;
            ++name;
        } else if (*pattern == '*') {
            star = pattern++;
            retry = name;
        } else if (star) {
            pattern = star + 1;
            name = ++retry;
        } else
            return 0;
    }
    while (*pattern == '*')
        ++pattern;
    return !*pattern;
}
s32 hm_card_directory(s32 port, const char *pattern, struct DIRENTRY *entries, long *count,
                      s32 offset, s32 maximum) {
    if (!count)
        return McErrCardInvalid;
    *count = 0;
    s32 status = hm_card_status(port);
    if (status)
        return status;
    if (!pattern || offset < 0 || maximum < 0 || (maximum && !entries))
        return McErrCardInvalid;
    int slot = hm_slot(port);
    for (int i = 0; i < HM_BLOCKS && *count < maximum; ++i) {
        unsigned char *e = hm_entry(hm_images[slot], i);
        if (!e[0] || !hm_matches(pattern, (char *)e))
            continue;
        if (offset) {
            --offset;
            continue;
        }
        struct DIRENTRY *out = &entries[(*count)++];
        memset(out, 0, sizeof(*out));
        memcpy(out->name, e, 21);
        out->size = (s32)hm_get(e + 24) * HM_BLOCK;
    }
    return McErrNone;
}
s32 hm_card_transfer(s32 port, const char *name, void *data, s32 offset, s32 bytes, int write) {
    s32 status = hm_card_status(port);
    if (status)
        return status;
    if (!hm_name(name) || !data || offset < 0 || bytes < 0)
        return McErrCardInvalid;
    int slot = hm_slot(port), index = hm_find(slot, name);
    if (index < 0)
        return McErrFileNotExist;
    unsigned char *e = hm_entry(hm_images[slot], index);
    unsigned size = hm_get(e + 24) * HM_BLOCK;
    if ((unsigned)offset > size || (unsigned)bytes > size - (unsigned)offset)
        return McErrCardInvalid;
    unsigned pos = HM_HEADER + hm_get(e + 28) * HM_BLOCK + (unsigned)offset;
    if (!write) {
        memcpy(data, hm_images[slot] + pos, (size_t)bytes);
        return McErrNone;
    }
    memcpy(hm_candidate, hm_images[slot], HM_CARD_BYTES);
    memcpy(hm_candidate + pos, data, (size_t)bytes);
    return hm_commit(slot);
}
