#include "actor.h"
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

#include "compat/nested/animals.inc"
#include "compat/nested/estate.inc"
#include "compat/nested/ground.inc"
#include "compat/nested/shop.inc"

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
    flags[9] = 3;
    flags[0] = 2;
    CHECK(has_medal_price(flags) == 1);
    return 0;
}
