#ifndef HMBTN_NPC_BONDS_MODEL_H
#define HMBTN_NPC_BONDS_MODEL_H

#include <stddef.h>

size_t hm_npc_bonds_count(void);
int hm_npc_bonds_character_id(size_t index);
const char *hm_npc_bonds_name(size_t index);
int hm_npc_bonds_uses_affection(size_t index);
int hm_npc_bonds_bar_width(int value, int maximum, int width);

#endif
