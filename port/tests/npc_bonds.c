#include "enhance/npc_bonds/model.h"
#include "villager.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Failed line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    static const int expected_ids[] = {
        CHARACTER_JEFF, CHARACTER_SASHA, CHARACTER_KAREN, CHARACTER_SAIBARA,
        CHARACTER_GRAY, CHARACTER_DOUG, CHARACTER_ANN, CHARACTER_DOCTOR,
        CHARACTER_ELLI, CHARACTER_PASTOR_CARTER, CHARACTER_CLIFF, CHARACTER_BASIL,
        CHARACTER_ANNA, CHARACTER_MARY, CHARACTER_BARLEY, CHARACTER_MAY,
        CHARACTER_MAYOR_THOMAS, CHARACTER_LILLIA, CHARACTER_RICK, CHARACTER_POPURI,
        CHARACTER_DUKE, CHARACTER_MANNA, CHARACTER_ELLEN, CHARACTER_GOTZ,
        CHARACTER_KAI, CHARACTER_HARRIS, CHARACTER_KANO, CHARACTER_GOURMET,
        CHARACTER_LOUIS, CHARACTER_GREG, CHARACTER_STU, CHARACTER_WON,
    };
    static const int affection_ids[] = {
        CHARACTER_KAREN, CHARACTER_ANN, CHARACTER_ELLI, CHARACTER_MARY, CHARACTER_POPURI,
    };

    CHECK(hm_npc_bonds_count() == sizeof(expected_ids) / sizeof(expected_ids[0]));
    for (size_t i = 0; i < hm_npc_bonds_count(); ++i) {
        CHECK(hm_npc_bonds_character_id(i) == expected_ids[i]);
        CHECK(hm_npc_bonds_name(i) != NULL && hm_npc_bonds_name(i)[0] != '\0');
        int uses_affection = 0;
        for (size_t j = 0; j < sizeof(affection_ids) / sizeof(affection_ids[0]); ++j)
            uses_affection |= expected_ids[i] == affection_ids[j];
        CHECK(hm_npc_bonds_uses_affection(i) == uses_affection);
    }
    CHECK(hm_npc_bonds_character_id(hm_npc_bonds_count()) == -1);
    CHECK(hm_npc_bonds_name(hm_npc_bonds_count()) == NULL);
    CHECK(hm_npc_bonds_uses_affection(hm_npc_bonds_count()) == 0);
    CHECK(strcmp(hm_npc_bonds_name(9), "PASTOR CARTER") == 0);
    CHECK(strcmp(hm_npc_bonds_name(16), "MAYOR THOMAS") == 0);

    CHECK(hm_npc_bonds_bar_width(-1, 255, 110) == 0);
    CHECK(hm_npc_bonds_bar_width(0, 255, 110) == 0);
    CHECK(hm_npc_bonds_bar_width(127, 255, 100) == 49);
    CHECK(hm_npc_bonds_bar_width(255, 255, 110) == 110);
    CHECK(hm_npc_bonds_bar_width(300, 255, 110) == 110);
    CHECK(hm_npc_bonds_bar_width(0x7FFF, 0xFFFF, 100) == 49);
    CHECK(hm_npc_bonds_bar_width(0xFFFF, 0xFFFF, 110) == 110);
    CHECK(hm_npc_bonds_bar_width(10, 0, 110) == 0);
    CHECK(hm_npc_bonds_bar_width(10, 10, 0) == 0);
    return 0;
}
