#include "actor.h"
#include "harvest_sprites.h"
#include "scene.h"
#include <libgte.h>
#include <stdio.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static VECTOR *ground_pos;
static s32 ground_dy, ground_tolerance, ground_pos_x, ground_pos_z;
static s32 ground_edge0, ground_edge1, ground_edge2, ground_found;
static SVECTOR *ground_triangle[3], *ground_vertices;
static s32 scene_flag_stage;

#include "compat/nested/animals.inc"
#include "compat/nested/estate.inc"
#include "compat/nested/ground.inc"
#include "compat/nested/scene.inc"
#include "compat/nested/shop.inc"

static FarmHelper *hm_waited_object;
static s32 hm_waited_proximity;
static s32 harvest_obj_wait_walk(FarmHelper *helper, s32 proximity) {
    hm_waited_object = helper;
    hm_waited_proximity = proximity;
    return 0;
}
#define HARVEST_NEAR_PROXIMITY 230
#define SLOT_NONE (-1)
#include "compat/nested/harvest.inc"
#undef harvest_helper_arrive_or_wait

int main(void) {
    SVECTOR vertices[4] = {{0, 0, 0, 0}, {10, 0, 0, 0}, {0, 0, 10, 0}, {10, 0, 10, 0}};
    VECTOR pos = {2, 0, 2, 0};
    ground_pos = &pos;
    ground_vertices = vertices;
    ground_tolerance = 10;
    CHECK(ground_triangle_under(vertices, vertices + 1, vertices + 2) == 0);
    CHECK(ground_found && ground_triangle[0] == vertices && ground_triangle[2] == vertices + 2);
    CHECK(ground_triangle_under(vertices + 2, vertices + 1, vertices) == 0);
    pos.vx = pos.vz = 5;
    CHECK(ground_triangle_under(vertices, vertices + 1, vertices + 2) == 0);
    pos.vx = pos.vz = 9;
    CHECK(ground_triangle_under(vertices, vertices + 1, vertices + 2) == 1);
    CHECK(ground_quad_under(0, 1, 2, 3) == 0 && ground_triangle[0] == vertices + 3);
    pos.vy = 11;
    CHECK(ground_quad_under(0, 1, 2, 3) == 1);
    pos.vy = -10;
    CHECK(ground_quad_under(0, 1, 2, 3) == 0);
    pos = (VECTOR){2, 0, 2, 0};
    CHECK(ground_quad_under(0, 1, 2, 1) == 0);

    FarmActor actor = {0};
    CHECK(farm_kind_matches_flags(&actor, 3) == 0);
    actor.flags = ACTOR_FLAG_DIED;
    CHECK(farm_kind_matches_flags(&actor, 1) == 0 && farm_kind_matches_flags(&actor, 2) == 1);
    actor.flags |= ACTOR_FLAG_DIED_OF_AGE;
    /* Preserve the original mask-versus-1 comparison, including its odd result.
     * Correcting this would change vanilla game behavior. */
    CHECK(farm_kind_matches_flags(&actor, 1) == 0 && farm_kind_matches_flags(&actor, 2) == 0);
    CHECK(farm_move_compare(INT32_MIN, INT32_MAX) == 1);
    CHECK(farm_move_compare(INT32_MAX, INT32_MIN) == 2);
    CHECK(farm_heading_compare(-10, -10) == 0);
    CHECK(farm_heading_compare(0, -1) == 2);

    CHECK(estate_kind_value_for_flags(0, 11, 22, 33) == 0);
    CHECK(estate_kind_value_for_flags(0x1000, 11, 22, 33) == 33);
    CHECK(estate_kind_value_for_flags(0x1800, 11, 22, 33) == 22);
    CHECK(estate_kind_value_for_flags(0x3800, 11, 22, 33) == 11);
    u8 flags[11] = {0};
    flags[10] = 2;
    CHECK(has_medal_price(flags) == 0); /* Only ten visible rows are considered. */
    flags[9] = 2;
    CHECK(has_medal_price(flags) == 1);
    FarmMorph first = {.step = 0, .steps = 2}, second = {.step = 7};
    FarmSceneModel owner = {.morph = &first},
                   other = {.flags = SCENE_ENTRY_HIDDEN, .morph = &second};
    scene_flag_stage = 4;
    CHECK(scene_slot_handoff_step(&owner, &other, 5) == 0);
    CHECK(first.step == 1 && second.step == 7 && scene_flag_stage == 4);
    CHECK(!(owner.flags & SCENE_ENTRY_HIDDEN) && (other.flags & SCENE_ENTRY_HIDDEN));
    CHECK(scene_slot_handoff_step(&owner, &other, 5) == 1);
    CHECK(first.step == 0 && second.step == 0 && scene_flag_stage == 5);
    CHECK((owner.flags & SCENE_ENTRY_HIDDEN) && !(other.flags & SCENE_ENTRY_HIDDEN));
    FarmHelper captured = {0}, carried = {0};
    carried.actor.place.location = 4;
    carried.actor.flags = 0;
    hm_harvest_helper_arrive_or_wait(&captured, &carried, 5, ACTOR_FLAG_BIT26);
    CHECK(hm_waited_object == &captured && hm_waited_proximity == 230);
    CHECK(carried.actor.flags & ACTOR_FLAG_BIT26);
    carried.targetAnimal = 8;
    carried.actor.taskId = 9;
    hm_waited_object = NULL;
    hm_harvest_helper_arrive_or_wait(&captured, &carried, 4, ACTOR_FLAG_BIT26);
    CHECK(carried.targetAnimal == -1 && carried.actor.taskId == 0);
    CHECK(!(carried.actor.flags & ACTOR_FLAG_BIT26) && (carried.flags & HELPER_BIT9));
    CHECK(hm_waited_object == NULL);
    flags[9] = 3;
    flags[0] = 2;
    CHECK(has_medal_price(flags) == 1);
    return 0;
}
