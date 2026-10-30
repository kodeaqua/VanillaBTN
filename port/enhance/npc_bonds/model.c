#include "enhance/npc_bonds/model.h"
#include "villager.h"
#include <stdint.h>

typedef struct {
    int character_id;
    const char *name;
    int uses_affection;
} hm_npc_bonds_entry;

static const hm_npc_bonds_entry hm_npc_bonds_entries[] = {
    {CHARACTER_JEFF, "JEFF", 0},
    {CHARACTER_SASHA, "SASHA", 0},
    {CHARACTER_KAREN, "KAREN", 1},
    {CHARACTER_SAIBARA, "SAIBARA", 0},
    {CHARACTER_GRAY, "GRAY", 0},
    {CHARACTER_DOUG, "DOUG", 0},
    {CHARACTER_ANN, "ANN", 1},
    {CHARACTER_DOCTOR, "DOCTOR", 0},
    {CHARACTER_ELLI, "ELLI", 1},
    {CHARACTER_PASTOR_CARTER, "PASTOR CARTER", 0},
    {CHARACTER_CLIFF, "CLIFF", 0},
    {CHARACTER_BASIL, "BASIL", 0},
    {CHARACTER_ANNA, "ANNA", 0},
    {CHARACTER_MARY, "MARY", 1},
    {CHARACTER_BARLEY, "BARLEY", 0},
    {CHARACTER_MAY, "MAY", 0},
    {CHARACTER_MAYOR_THOMAS, "MAYOR THOMAS", 0},
    {CHARACTER_LILLIA, "LILLIA", 0},
    {CHARACTER_RICK, "RICK", 0},
    {CHARACTER_POPURI, "POPURI", 1},
    {CHARACTER_DUKE, "DUKE", 0},
    {CHARACTER_MANNA, "MANNA", 0},
    {CHARACTER_ELLEN, "ELLEN", 0},
    {CHARACTER_GOTZ, "GOTZ", 0},
    {CHARACTER_KAI, "KAI", 0},
    {CHARACTER_HARRIS, "HARRIS", 0},
    {CHARACTER_KANO, "KANO", 0},
    {CHARACTER_GOURMET, "GOURMET", 0},
    {CHARACTER_LOUIS, "LOUIS", 0},
    {CHARACTER_GREG, "GREG", 0},
    {CHARACTER_STU, "STU", 0},
    {CHARACTER_WON, "WON", 0},
};

size_t hm_npc_bonds_count(void)
{
    return sizeof(hm_npc_bonds_entries) / sizeof(hm_npc_bonds_entries[0]);
}

int hm_npc_bonds_character_id(size_t index)
{
    if (index >= hm_npc_bonds_count())
        return -1;
    return hm_npc_bonds_entries[index].character_id;
}

const char *hm_npc_bonds_name(size_t index)
{
    if (index >= hm_npc_bonds_count())
        return NULL;
    return hm_npc_bonds_entries[index].name;
}

int hm_npc_bonds_uses_affection(size_t index)
{
    if (index >= hm_npc_bonds_count())
        return 0;
    return hm_npc_bonds_entries[index].uses_affection;
}

int hm_npc_bonds_bar_width(int value, int maximum, int width)
{
    if (maximum <= 0 || width <= 0 || value <= 0)
        return 0;
    if (value > maximum)
        value = maximum;
    return (int)(((int64_t)value * width) / maximum);
}
