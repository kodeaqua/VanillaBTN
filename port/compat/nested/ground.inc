/* Native copies of the repository's CC0 nested helpers. No algorithm changes. */
static s32 ground_triangle_under(SVECTOR *a, SVECTOR *b, SVECTOR *c) {
    VECTOR *pos;

    pos = ground_pos;
    ground_dy = (a->vy + b->vy + c->vy) / 3;
    ground_dy = pos->vy - ground_dy;
    if (ground_dy > ground_tolerance || ground_dy < -ground_tolerance) {
        return 1;
    }
    ground_pos_x = pos->vx;
    ground_pos_z = pos->vz;
    ground_edge0 =
        (b->vx - a->vx) * (ground_pos_z - a->vz) - (b->vz - a->vz) * (ground_pos_x - a->vx);
    ground_edge1 =
        (c->vx - b->vx) * (ground_pos_z - b->vz) - (c->vz - b->vz) * (ground_pos_x - b->vx);
    ground_edge2 =
        (a->vx - c->vx) * (ground_pos_z - c->vz) - (a->vz - c->vz) * (ground_pos_x - c->vx);
    if ((ground_edge0 > 0 || ground_edge1 > 0 || ground_edge2 > 0) &&
        (ground_edge0 < 0 || ground_edge1 < 0 || ground_edge2 < 0)) {
        return 1;
    }
    ground_found = 1;
    ground_triangle[0] = a;
    ground_triangle[1] = b;
    ground_triangle[2] = c;
    return 0;
}

static s32 ground_quad_under(s32 a, s32 b, s32 c, s32 d) {
    if (a == b || a == c) {
        return ground_triangle_under(&ground_vertices[c], &ground_vertices[b], &ground_vertices[d]);
    }
    if (d == b || d == c) {
        return ground_triangle_under(&ground_vertices[a], &ground_vertices[b], &ground_vertices[c]);
    }
    if (ground_triangle_under(&ground_vertices[a], &ground_vertices[b], &ground_vertices[c]) == 0) {
        return 0;
    }
    return ground_triangle_under(&ground_vertices[d], &ground_vertices[c], &ground_vertices[b]);
}
