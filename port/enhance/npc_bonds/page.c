#include "enhance/npc_bonds/page.h"
#include "enhance/npc_bonds/face.h"
#include "enhance/npc_bonds/model.h"
#include "afile.h"
#include "farm_state.h"
#include "main.h"
#include "pad.h"
#include "prim.h"
#include "sound_request.h"
#include "status_pages.h"
#include "text.h"
#include "villager.h"

#define HM_NPC_BONDS_ROWS 7
#define HM_NPC_BONDS_ROW_Y 61
#define HM_NPC_BONDS_ROW_STEP 20
#define HM_NPC_BONDS_BAR_WIDTH 110
#define HM_NPC_BONDS_FACE_BANK_STRIDE 0x1800

extern s16 status_list_top;
extern s32 afile_read_active;

static u8 hm_npc_bonds_face_bank_data[HM_NPC_BONDS_FACE_BANK_STRIDE];
static uint32_t hm_npc_bonds_faces[HM_NPC_BONDS_COUNT][HM_NPC_BONDS_FACE_PIXELS];
static u8 hm_npc_bonds_face_state[HM_NPC_BONDS_COUNT];
static int hm_npc_bonds_face_reading = -1;

static u16 hm_npc_bonds_glyph(char value)
{
    if (value >= 'A' && value <= 'Z')
        return (u16)(value - 'A');
    if (value >= 'a' && value <= 'z')
        return (u16)(0x20 + value - 'a');
    if (value >= '0' && value <= '9')
        return (u16)(0x40 + value - '0');
    return 0x0FFD;
}

static void hm_npc_bonds_text(u16 *glyphs, size_t capacity, const char *text)
{
    size_t index = 0;

    if (capacity == 0)
        return;
    while (text[index] != '\0' && index + 1 < capacity) {
        glyphs[index] = hm_npc_bonds_glyph(text[index]);
        index++;
    }
    glyphs[index] = 0xFFFF;
}

static void hm_npc_bonds_name_text(u16 *glyphs, size_t capacity, const char *text)
{
    size_t index = 0;
    int capitalize = 1;

    if (capacity == 0)
        return;
    while (text[index] != '\0' && index + 1 < capacity) {
        const char value = text[index];

        if (value >= 'A' && value <= 'Z') {
            glyphs[index] = (u16)((capitalize ? 0 : 0x20) + value - 'A');
            capitalize = 0;
        } else {
            glyphs[index] = hm_npc_bonds_glyph(value);
            capitalize = value == ' ';
        }
        index++;
    }
    glyphs[index] = 0xFFFF;
}

static void hm_npc_bonds_update_faces(void)
{
    u8 *loaded;
    int row;

    if (hm_npc_bonds_face_reading >= 0) {
        const int index = hm_npc_bonds_face_reading;
        const int bank = hm_npc_bonds_face_bank((size_t)index);

        loaded = afile_read_bytes(1, bank * HM_NPC_BONDS_FACE_BANK_STRIDE,
                                  HM_NPC_BONDS_FACE_BANK_STRIDE,
                                  hm_npc_bonds_face_bank_data, 1, 0);
        if (loaded) {
            hm_npc_bonds_face_state[index] =
                (u8)hm_npc_bonds_decode_face_bank(
                    loaded, HM_NPC_BONDS_FACE_BANK_STRIDE, hm_npc_bonds_faces[index]);
            if (!hm_npc_bonds_face_state[index])
                hm_npc_bonds_face_state[index] = 2;
            hm_npc_bonds_face_reading = -1;
        }
        return;
    }

    if (afile_read_active)
        return;

    for (row = 0; row < HM_NPC_BONDS_ROWS; ++row) {
        const size_t index = (size_t)status_list_top + (size_t)row;
        int bank;

        if (index >= hm_npc_bonds_count())
            break;
        bank = hm_npc_bonds_face_bank(index);
        if (hm_npc_bonds_face_state[index] != 0 || bank < 0)
            continue;

        hm_npc_bonds_face_reading = (int)index;
        afile_read_bytes(1, bank * HM_NPC_BONDS_FACE_BANK_STRIDE,
                         HM_NPC_BONDS_FACE_BANK_STRIDE,
                         hm_npc_bonds_face_bank_data, 1, 1);
        return;
    }
}

static void hm_npc_bonds_draw_avatar(u_long *ot, int x, int y, size_t index)
{
    const uint32_t *pixels = hm_npc_bonds_faces[index];
    int row;

    if (hm_npc_bonds_face_state[index] == 1) {
        for (row = 0; row < HM_NPC_BONDS_FACE_SIZE; ++row) {
            int column = 0;
            while (column < HM_NPC_BONDS_FACE_SIZE) {
                const uint32_t color = pixels[row * HM_NPC_BONDS_FACE_SIZE + column];
                int end = column + 1;

                if (color == 0) {
                    column++;
                    continue;
                }
                while (end < HM_NPC_BONDS_FACE_SIZE &&
                       pixels[row * HM_NPC_BONDS_FACE_SIZE + end] == color)
                    end++;
                prim_add_rect(ot, (s16)(x + column), (s16)(y + row),
                              (s16)(end - column), 1, (s32)color, 0);
                column = end;
            }
        }
    }

    /* Add the backing last so the face pixels appear over it in OT order. */
    prim_add_rect(ot, (s16)x, (s16)y, HM_NPC_BONDS_FACE_SIZE,
                  HM_NPC_BONDS_FACE_SIZE, 0x4A4038, 0);
}

static void hm_npc_bonds_draw_row(size_t index, int y)
{
    const int character_id = hm_npc_bonds_character_id(index);
    const int affection = hm_npc_bonds_uses_affection(index);
    const FarmVillager *villager = &game_state.villagers[character_id];
    const int value = affection ? villager->affection : villager->friendship;
    const int maximum = affection ? 0xFFFF : FRIENDSHIP_MAX;
    const int fill = hm_npc_bonds_bar_width(value, maximum, HM_NPC_BONDS_BAR_WIDTH);
    u16 name[20];
    const char *name_text = hm_npc_bonds_name(index);
    u_long *text_ot = &current_frame->otTags[0][21];
    u_long *prim_ot = &current_frame->otTags[0][26];

    hm_npc_bonds_name_text(name, sizeof(name) / sizeof(name[0]), name_text);
    hm_npc_bonds_draw_avatar(prim_ot, 28, y + 1, index);
    text_draw_plain(text_ot, name, 51, y + 6);

    if (fill > 0) {
        prim_add_rect(prim_ot, 158, (s16)(y + 7), (s16)fill, 4,
                      affection ? 0xE47FA8 : 0x69B982, 0);
    }
    prim_add_rect(prim_ot, 156, (s16)(y + 6), 114, 6, 0x6E6256, 0);
    prim_add_rect(prim_ot, 155, (s16)(y + 5), 116, 8, 0x302D31, 0);
}

void hm_npc_bonds_draw_page(void)
{
    static const char *const heading = "Relations";
    static const char *const friendship_label = "Friends";
    static const char *const affection_label = "Love";
    const size_t count = hm_npc_bonds_count();
    const int max_top = (int)count - HM_NPC_BONDS_ROWS;
    u16 text[16];
    int row;

    hm_npc_bonds_text(text, sizeof(text) / sizeof(text[0]), heading);
    text_draw_plain(&current_frame->otTags[0][21], text, 176, 26);

    if (input_pad1->triggered & PAD_UP) {
        sound_play(SOUND_MENU_CURSOR);
        status_list_top--;
    } else if (input_pad1->triggered & PAD_DOWN) {
        sound_play(SOUND_MENU_CURSOR);
        status_list_top++;
    }
    if (status_list_top < 0)
        status_list_top = (s16)max_top;
    else if (status_list_top > max_top)
        status_list_top = 0;

    hm_npc_bonds_update_faces();

    hm_npc_bonds_text(text, sizeof(text) / sizeof(text[0]), friendship_label);
    text_draw_plain(&current_frame->otTags[0][21], text, 151, 50);
    hm_npc_bonds_text(text, sizeof(text) / sizeof(text[0]), affection_label);
    text_draw_plain(&current_frame->otTags[0][21], text, 216, 50);
    prim_add_rect(&current_frame->otTags[0][26], 202, 51, 6, 4, 0x69B982, 0);
    prim_add_rect(&current_frame->otTags[0][26], 252, 51, 6, 4, 0xE47FA8, 0);

    if (status_list_top > 0)
        status_draw_arrow_up();
    if (status_list_top + HM_NPC_BONDS_ROWS < (int)count)
        status_draw_arrow_down();

    for (row = 0; row < HM_NPC_BONDS_ROWS; row++) {
        size_t index = (size_t)status_list_top + (size_t)row;
        if (index >= count)
            break;
        hm_npc_bonds_draw_row(index, HM_NPC_BONDS_ROW_Y + row * HM_NPC_BONDS_ROW_STEP);
    }
}
