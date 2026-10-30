#include "enhance/npc_bonds/page.h"
#include "enhance/npc_bonds/model.h"
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

extern s16 status_list_top;

static u16 hm_npc_bonds_glyph(char value)
{
    if (value >= 'A' && value <= 'Z')
        return (u16)(value - 'A');
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

static void hm_npc_bonds_draw_avatar(u_long *ot, int x, int y, int character_id)
{
    static const s32 backgrounds[] = {
        0x758B79, 0x7187A0, 0x9A7E71, 0x947B91, 0x8C9869, 0x7B9394,
    };
    static const s32 hair_colors[] = {
        0x47352D, 0x34313A, 0x76503A, 0x51483E,
    };
    static const s32 skin_colors[] = {
        0xD5AC82, 0xE0B990, 0xC99570,
    };
    static const s32 shirt_colors[] = {
        0x557A68, 0x526C8A, 0x8B6258, 0x74628C,
    };
    unsigned int id = (unsigned int)character_id;
    s32 hair = hair_colors[id % (sizeof(hair_colors) / sizeof(hair_colors[0]))];
    s32 skin = skin_colors[id % (sizeof(skin_colors) / sizeof(skin_colors[0]))];
    s32 shirt = shirt_colors[id % (sizeof(shirt_colors) / sizeof(shirt_colors[0]))];

    prim_add_rect(ot, (s16)(x + 6), (s16)(y + 7), 1, 1, 0x342C28, 0);
    prim_add_rect(ot, (s16)(x + 11), (s16)(y + 7), 1, 1, 0x342C28, 0);
    prim_add_rect(ot, (s16)(x + 4), (s16)(y + 2), 10, 4, hair, 0);
    prim_add_rect(ot, (s16)(x + 12), (s16)(y + 4), 3, 8, hair, 0);
    prim_add_rect(ot, (s16)(x + 3), (s16)(y + 4), 3, 8, hair, 0);
    prim_add_rect(ot, (s16)(x + 4), (s16)(y + 3), 10, 9, skin, 0);
    prim_add_rect(ot, (s16)(x + 7), (s16)(y + 10), 4, 3, skin, 0);
    prim_add_rect(ot, (s16)(x + 2), (s16)(y + 12), 14, 6, shirt, 0);
    prim_add_rect(ot, (s16)x, (s16)y, 18, 18,
                  backgrounds[id % (sizeof(backgrounds) / sizeof(backgrounds[0]))], 0);
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

    hm_npc_bonds_text(name, sizeof(name) / sizeof(name[0]), name_text);
    hm_npc_bonds_draw_avatar(prim_ot, 28, y + 1, character_id);
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
    static const char *const heading = "RELATIONS";
    static const char *const friendship_label = "FRIEND";
    static const char *const affection_label = "LOVE";
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
