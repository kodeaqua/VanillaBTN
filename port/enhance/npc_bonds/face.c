#include "enhance/npc_bonds/face.h"

#include <string.h>

#define HM_FACE_BANK_TYPE 0x0D
#define HM_FACE_PALETTE_X 0
#define HM_FACE_PALETTE_Y 490
#define HM_FACE_PALETTE_WIDTH 256
#define HM_FACE_PALETTE_HEIGHT 1
#define HM_FACE_TEXTURE_X 960
#define HM_FACE_TEXTURE_Y 175
#define HM_FACE_TEXTURE_WIDTH 40
#define HM_FACE_TEXTURE_HEIGHT 80
#define HM_FACE_TEXTURE_ROW_BYTES (HM_FACE_TEXTURE_WIDTH * 2)
#define HM_FACE_TEXTURE_BYTES (HM_FACE_TEXTURE_ROW_BYTES * HM_FACE_TEXTURE_HEIGHT)
#define HM_FACE_PALETTE_BYTES (HM_FACE_PALETTE_WIDTH * 2)
#define HM_FACE_BANK_IMAGE_HEADER_SIZE 12
#define HM_FACE_BANK_STRIDE 0x1800

static uint16_t hm_face_read_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static uint32_t hm_face_read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint32_t hm_face_rgb(uint16_t color)
{
    const uint32_t red = color & 0x1F;
    const uint32_t green = (color >> 5) & 0x1F;
    const uint32_t blue = (color >> 10) & 0x1F;

    return ((red * 255 / 31) << 16) | ((green * 255 / 31) << 8) |
           (blue * 255 / 31);
}

static int hm_face_decode_rle(const uint8_t *source, size_t source_size,
                              uint8_t *destination, size_t destination_size)
{
    size_t source_index = 0;
    size_t destination_index = 0;

    while (destination_index < destination_size) {
        int8_t run;
        size_t count;

        if (source_index >= source_size)
            return 0;
        run = (int8_t)source[source_index++];
        if (run < 0) {
            count = (size_t)(-run);
            if (count > source_size - source_index ||
                count > destination_size - destination_index)
                return 0;
            memcpy(destination + destination_index, source + source_index, count);
            source_index += count;
        } else {
            if (run == 0 || source_index >= source_size ||
                (size_t)run > destination_size - destination_index)
                return 0;
            count = (size_t)run;
            memset(destination + destination_index, source[source_index++], count);
        }
        destination_index += count;
    }

    return 1;
}

static int hm_face_image(const uint8_t *bank, size_t bank_size, size_t offset,
                         uint32_t *size_flags, uint16_t *x, uint16_t *y,
                         uint16_t *width, uint16_t *height, const uint8_t **data,
                         size_t *next_offset)
{
    size_t data_offset;
    size_t data_size;
    size_t padded_size;

    if (offset > bank_size || bank_size - offset < HM_FACE_BANK_IMAGE_HEADER_SIZE)
        return 0;

    *size_flags = hm_face_read_u32(bank + offset);
    *x = hm_face_read_u16(bank + offset + 4);
    *y = hm_face_read_u16(bank + offset + 6);
    *width = hm_face_read_u16(bank + offset + 8);
    *height = hm_face_read_u16(bank + offset + 10);
    data_size = *size_flags & 0x00FFFFFF;
    data_offset = offset + HM_FACE_BANK_IMAGE_HEADER_SIZE;
    padded_size = (data_size + 3) & ~(size_t)3;
    if (padded_size > bank_size - data_offset)
        return 0;

    *data = bank + data_offset;
    *next_offset = data_offset + padded_size;
    return 1;
}

int hm_npc_bonds_decode_face_bank(const uint8_t *bank, size_t bank_size,
                                  uint32_t pixels[HM_NPC_BONDS_FACE_PIXELS])
{
    uint32_t palette_flags, texture_flags;
    uint16_t x, y, width, height;
    const uint8_t *palette_data, *texture_data;
    size_t next_offset;
    uint16_t palette[HM_FACE_PALETTE_WIDTH];
    uint8_t texture[HM_FACE_TEXTURE_BYTES];
    size_t row;
    size_t column;

    if (!bank || !pixels || bank_size < HM_FACE_BANK_STRIDE ||
        bank[0] != HM_FACE_BANK_TYPE || hm_face_read_u16(bank + 2) != 2)
        return 0;

    if (!hm_face_image(bank, bank_size, 4, &palette_flags, &x, &y, &width,
                       &height, &palette_data, &next_offset))
        return 0;
    if (x != HM_FACE_PALETTE_X || y != HM_FACE_PALETTE_Y ||
        width != HM_FACE_PALETTE_WIDTH || height != HM_FACE_PALETTE_HEIGHT ||
        (palette_flags >> 24 & 0x0C) != 0x04 ||
        (palette_flags & 0x00FFFFFF) != HM_FACE_PALETTE_BYTES)
        return 0;
    for (column = 0; column < HM_FACE_PALETTE_WIDTH; column++)
        palette[column] = hm_face_read_u16(palette_data + column * 2);

    if (!hm_face_image(bank, bank_size, next_offset, &texture_flags, &x, &y,
                       &width, &height, &texture_data, &next_offset))
        return 0;
    if (x != HM_FACE_TEXTURE_X || y != HM_FACE_TEXTURE_Y ||
        width != HM_FACE_TEXTURE_WIDTH || height != HM_FACE_TEXTURE_HEIGHT ||
        (texture_flags >> 24 & 0x0C) != 0 ||
        !hm_face_decode_rle(texture_data, texture_flags & 0x00FFFFFF,
                            texture, sizeof(texture)))
        return 0;

    for (row = 0; row < HM_NPC_BONDS_FACE_SIZE; row++) {
        const size_t source_y = row * 60 / HM_NPC_BONDS_FACE_SIZE;
        for (column = 0; column < HM_NPC_BONDS_FACE_SIZE; column++) {
            const size_t source_x = 8 + column * 64 / HM_NPC_BONDS_FACE_SIZE;
            const size_t source_pixel = source_y * 80 + source_x;
            const uint16_t word = hm_face_read_u16(texture + (source_pixel & ~(size_t)1));
            const uint8_t palette_index = (source_pixel & 1) ? (uint8_t)(word >> 8) : (uint8_t)word;

            pixels[row * HM_NPC_BONDS_FACE_SIZE + column] =
                palette_index == 0 ? 0 : hm_face_rgb(palette[palette_index]);
        }
    }

    return 1;
}
