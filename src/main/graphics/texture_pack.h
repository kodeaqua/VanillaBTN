#ifndef TEXTURE_PACK_H
#define TEXTURE_PACK_H

/* Texture packs and image banks (a 0x0D texture pack): their layout, and uploading them to
 * VRAM. */

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* One image of an image bank: a 24-bit data size and 8 flag bits, the VRAM
 * rectangle, then the pixel data, padded to a word. */
typedef struct BankImage {
    u32 sizeFlags;      /* data bytes (low 24 bits) | flags << 24 */
    u16 x, y;
    u16 w, h;           /* w in halfwords */
#ifdef VANILLABTN_NATIVE
    /* A one-byte trailing placeholder is required for arrays of BankImage on
     * MSVC. BANK_NEXT uses the explicit 12-byte file header, not sizeof. */
    u8 data[1];
#else
    u8 data[0];
#endif
} BankImage;

/* An image bank: a header, then `count` BankImages. The images differ in
 * length, so only the first is at `images`; BANK_NEXT steps to the next. */
typedef struct ImageBank {
    u8 type;            /* 0xD */
    u8 pad01;
    u16 count;
#ifdef VANILLABTN_NATIVE
    /* MSVC rejects zero-length arrays whose element also has a trailing array.
     * Only the first image's offset is used; never sizeof(ImageBank) on disk. */
    BankImage images[1];
#else
    BankImage images[0];
#endif
} ImageBank;

/* The image after `image`, of `size` data bytes: the data padded to a word, then the
 * BankImage header. */
#define BANK_IMAGE_HEADER_SIZE 0xC
#define BANK_NEXT(image, size) ((BankImage *)((u8 *)(image) + (((size) + 3) / 4 * 4 + BANK_IMAGE_HEADER_SIZE)))

void texture_pack_load(u8 *pack);
void texture_pack_load_at(void *pack, s16 x, s16 y, s16 altX, s16 altY);
void texture_pack_load_keep_wide(void *pack, s16 x, s16 y, s16 altX, s16 altY);

s32 image_bank_upload(void *bank, u8 *buffer, u16 index, s16 x, s16 y);

#endif /* TEXTURE_PACK_H */
