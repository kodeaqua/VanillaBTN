#include "psx/disc.h"
#include "psx/sha1.h"
#include <string.h>

static u32 hm_le32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u32 hm_be32(const u8 *p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

const char *hm_disc_error(hm_disc_result result) {
    switch (result) {
    case HM_DISC_OK:
        return "Disc operation completed";
    case HM_DISC_IO:
        return "Cannot read the disc image";
    case HM_DISC_WRONG_IMAGE:
        return "Wrong disc image: use the original USA SLUS_011.15 BIN track";
    case HM_DISC_FORMAT:
        return "Invalid disc sector or ISO-9660 directory";
    case HM_DISC_NOT_FOUND:
        return "Requested file is absent from the disc";
    case HM_DISC_RANGE:
        return "Disc read is outside the file or track";
    case HM_DISC_UNSUPPORTED:
        return "This disc record or sector format is not supported yet";
    }
    return "Unknown disc error";
}

hm_disc_result hm_disc_decode_sector(const u8 raw[2352], u8 data[2048]) {
    static const u8 sync[12] = {0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0};
    if (memcmp(raw, sync, sizeof(sync)))
        return HM_DISC_FORMAT;
    if (raw[15] == 1) {
        memcpy(data, raw + 16, 2048);
        return HM_DISC_OK;
    }
    if (raw[15] != 2)
        return HM_DISC_UNSUPPORTED;
    if (memcmp(raw + 16, raw + 20, 4))
        return HM_DISC_FORMAT;
    /* Form 2 carries 2324 bytes, not a 2048-byte filesystem block. XA audio
     * must use a separate decoder rather than silently truncating its payload. */
    if (raw[18] & 0x20)
        return HM_DISC_UNSUPPORTED;
    memcpy(data, raw + 24, 2048);
    return HM_DISC_OK;
}

hm_disc_result hm_disc_sector(hm_disc *disc, u32 lba, void *destination) {
    if (!disc->mounted || lba >= disc->sectors)
        return HM_DISC_RANGE;
    u8 raw[2352];
    if (!disc->stream.read_at(disc->stream.context, (u64)lba * 2352, raw, sizeof(raw)))
        return HM_DISC_IO;
    return hm_disc_decode_sector(raw, destination);
}

static hm_disc_result hm_record(hm_disc *disc, const u8 *record, size_t available,
                                hm_disc_entry *entry) {
    if (available < 34 || record[0] < 34 || record[0] > available || !record[32] ||
        (size_t)33 + record[32] + !(record[32] & 1) > record[0])
        return HM_DISC_FORMAT;
    if (hm_le32(record + 2) != hm_be32(record + 6) || hm_le32(record + 10) != hm_be32(record + 14))
        return HM_DISC_FORMAT;
    if (record[1] || record[26] || record[27] || (record[25] & 0x80))
        return HM_DISC_UNSUPPORTED;
    *entry = (hm_disc_entry){hm_le32(record + 2), hm_le32(record + 10), record[25]};
    if (entry->sector >= disc->sectors ||
        ((u64)entry->size + 2047) / 2048 > disc->sectors - entry->sector)
        return HM_DISC_RANGE;
    return HM_DISC_OK;
}

hm_disc_result hm_disc_mount(hm_disc *disc, hm_disc_stream stream) {
    /* Expected raw track hash from disc/disc.sha1; no proprietary bytes here. */
    static const u8 expected[20] = {0xcb, 0x26, 0x38, 0x1f, 0xa1, 0x33, 0x6b, 0xdd, 0x15, 0xf0,
                                    0x74, 0x69, 0xe5, 0x9f, 0x11, 0x43, 0xcc, 0xf3, 0x7b, 0x03};
    *disc = (hm_disc){0};
    if (!stream.read_at || !stream.size || stream.size % 2352 || stream.size / 2352 > UINT32_MAX)
        return HM_DISC_WRONG_IMAGE;
    hm_sha1 hash;
    hm_sha1_init(&hash);
    u8 buffer[65536], digest[20];
    for (u64 offset = 0; offset < stream.size;) {
        size_t take = sizeof(buffer);
        if (stream.size - offset < take)
            take = (size_t)(stream.size - offset);
        if (!stream.read_at(stream.context, offset, buffer, take))
            return HM_DISC_IO;
        hm_sha1_update(&hash, buffer, take);
        offset += take;
    }
    hm_sha1_finish(&hash, digest);
    if (memcmp(digest, expected, sizeof(digest)))
        return HM_DISC_WRONG_IMAGE;
    disc->stream = stream;
    disc->sectors = (u32)(stream.size / 2352);
    disc->mounted = 1;
    u8 pvd[2048];
    hm_disc_result result = hm_disc_sector(disc, 16, pvd);
    if (result == HM_DISC_OK) {
        if (pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) || pvd[6] != 1 || pvd[128] != 0 ||
            pvd[129] != 8 || pvd[130] != 8 || pvd[131] != 0 ||
            hm_le32(pvd + 80) != hm_be32(pvd + 84) || hm_le32(pvd + 80) > disc->sectors)
            result = HM_DISC_FORMAT;
        else
            result = hm_record(disc, pvd + 156, 34, &disc->root);
        if (result == HM_DISC_OK && !(disc->root.flags & 2))
            result = HM_DISC_FORMAT;
    }
    if (result != HM_DISC_OK)
        *disc = (hm_disc){0};
    return result;
}

static int hm_name_equal(const u8 *record, const char *name, size_t size) {
    if (record[32] != size)
        return 0;
    for (size_t i = 0; i < size; ++i) {
        u8 c = (u8)name[i];
        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        if (record[33 + i] != c)
            return 0;
    }
    return 1;
}

hm_disc_result hm_disc_find(hm_disc *disc, const char *path, hm_disc_entry *entry) {
    if (!disc->mounted)
        return HM_DISC_RANGE;
    hm_disc_entry current = disc->root;
    while (*path == '/' || *path == '\\')
        ++path;
    while (*path) {
        if (!(current.flags & 2))
            return HM_DISC_NOT_FOUND;
        const char *name = path;
        while (*path && *path != '/' && *path != '\\')
            ++path;
        size_t length = (size_t)(path - name);
        if (length > 255 || (length == 1 && name[0] == '.') ||
            (length == 2 && name[0] == '.' && name[1] == '.'))
            return HM_DISC_NOT_FOUND;
        int found = 0;
        for (u64 offset = 0; offset < current.size && !found; offset += 2048) {
            u8 block[2048];
            hm_disc_result result =
                hm_disc_sector(disc, current.sector + (u32)(offset / 2048), block);
            if (result != HM_DISC_OK)
                return result;
            size_t available =
                current.size - offset < 2048 ? (size_t)(current.size - offset) : 2048;
            for (size_t at = 0; at < available && block[at]; at += block[at]) {
                hm_disc_entry candidate;
                result = hm_record(disc, block + at, available - at, &candidate);
                if (result != HM_DISC_OK)
                    return result;
                if (hm_name_equal(block + at, name, length)) {
                    current = candidate;
                    found = 1;
                    break;
                }
            }
        }
        if (!found)
            return HM_DISC_NOT_FOUND;
        while (*path == '/' || *path == '\\')
            ++path;
    }
    *entry = current;
    return HM_DISC_OK;
}

hm_disc_result hm_disc_read(hm_disc *disc, const hm_disc_entry *entry, u32 offset,
                            void *destination, size_t size) {
    if (!disc->mounted || offset > entry->size || size > entry->size - offset)
        return HM_DISC_RANGE;
    u8 *out = destination;
    while (size) {
        u8 block[2048];
        u32 lba_offset = offset / 2048;
        if (entry->sector >= disc->sectors || lba_offset >= disc->sectors - entry->sector)
            return HM_DISC_RANGE;
        hm_disc_result result = hm_disc_sector(disc, entry->sector + lba_offset, block);
        if (result != HM_DISC_OK)
            return result;
        size_t start = offset % 2048, take = 2048 - start;
        if (take > size)
            take = size;
        memcpy(out, block + start, take);
        out += take;
        offset += (u32)take;
        size -= take;
    }
    return HM_DISC_OK;
}
