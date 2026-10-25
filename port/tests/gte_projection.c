#include <inline_c.h>
#include <libgpu.h>
#include <stdio.h>
#include <string.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    InitGeom();
    CHECK(hm_gte_divide(&hm_gte, 1, 1) == 65536 && hm_gte.flags == 0);
    CHECK(hm_gte_divide(&hm_gte, 0, 1) == 0 && hm_gte.flags == 0);
    CHECK(hm_gte_divide(&hm_gte, 0xFE3F, 0x7F20) == 0x1FFFF && hm_gte.flags == 0);
    CHECK(hm_gte_divide(&hm_gte, 0xF015, 0x780B) == 0x1FFFF && hm_gte.flags == 0);
    CHECK(hm_gte_divide(&hm_gte, 1000, 500) == 0x1FFFF && hm_gte.flags == (1u << 17));

    InitGeom();
    SetRotMatrix(&GsIDMATRIX);
    SetTransMatrix(&GsIDMATRIX);
    SetGeomOffset(160, 120);
    SetGeomScreen(1000);
    SVECTOR vertex = {100, -50, 1000, 0};
    gte_ldv0(&vertex);
    hm_gte_command(&hm_gte, 0x4A180001);
    CHECK(hm_gte.sx[2] == 260 && hm_gte.sy[2] == 70 && hm_gte.sz[3] == 1000);
    CHECK(hm_gte.mac[1] == 100 && hm_gte.mac[2] == -50 && hm_gte.mac[3] == 1000);
    CHECK(hm_gte.ir0 == 0 && !hm_gte.flags);
    u32 xy = 0;
    gte_stsxy(&xy);
    CHECK(xy == 0x00460104);
    SVECTOR vertices[3] = {{100, 0, 1000, 0}, {0, 100, 1000, 0}, {-100, 0, 1000, 0}};
    gte_ldv3(vertices, vertices + 1, vertices + 2);
    hm_gte_command(&hm_gte, 0x4A280030);
    CHECK(hm_gte.sx[0] == 260 && hm_gte.sx[1] == 160 && hm_gte.sx[2] == 60);
    CHECK(hm_gte.sy[0] == 120 && hm_gte.sy[1] == 220 && hm_gte.sy[2] == 120);
    CHECK(hm_gte.sz[0] == 1000 && hm_gte.sz[3] == 1000 && !hm_gte.flags);
    POLY_GT4 packet;
    memset(&packet, 0xA5, sizeof(packet));
    gte_stsxy3_gt4(&packet);
    CHECK(packet.x0 == 260 && packet.y0 == 120 && packet.x1 == 160 && packet.y1 == 220);
    CHECK(packet.x2 == 60 && packet.y2 == 120 && packet.x3 == (s16)0xA5A5);
    CHECK(packet.r0 == 0xA5 && packet.r1 == 0xA5 && packet.r2 == 0xA5 && packet.u0 == 0xA5);

    PushMatrix();
    MATRIX moved = GsIDMATRIX;
    moved.m[0][0] = 2048;
    moved.t[0] = 100;
    SetRotMatrix(&moved);
    SetTransMatrix(&moved);
    VECTOR result = {.pad = 123};
    s32 flags;
    RotTrans(&vertex, &result, &flags);
    CHECK(result.vx == 150 && result.vy == -50 && result.vz == 1000 && result.pad == 123 && !flags);
    PopMatrix();
    MATRIX restored;
    gte_ReadRotMatrix(&restored);
    CHECK(restored.m[0][0] == 4096 && restored.t[0] == 0);
    MATRIX aliased = GsIDMATRIX;
    aliased.m[0][0] = 2048;
    gte_MulMatrix0(&aliased, &aliased, &aliased);
    CHECK(aliased.m[0][0] == 1024 && aliased.m[1][1] == 4096);

    InitGeom();
    SetRotMatrix(&GsIDMATRIX);
    SetGeomScreen(1000);
    vertex = (SVECTOR){32767, -32768, 1000, 0};
    gte_ldv0(&vertex);
    hm_gte_command(&hm_gte, 0x4A180001);
    CHECK(hm_gte.sx[2] == 1023 && hm_gte.sy[2] == -1024 && hm_gte.flags == 0x80006000);
    vertex = (SVECTOR){0, 0, -1, 0};
    gte_ldv0(&vertex);
    hm_gte_command(&hm_gte, 0x4A180401); /* lm=1 stores zero IR3 without IR3 flag. */
    CHECK(hm_gte.ir[2] == 0 && hm_gte.sz[3] == 0 && hm_gte.flags == 0x80060000);
    return 0;
}
