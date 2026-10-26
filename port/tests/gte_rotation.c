#include <libgte.h>
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
    MATRIX out = {.t = {1, 2, 3}};
    SVECTOR angle = {0};
    CHECK(RotMatrix(&angle, &out) == &out);
    CHECK(!memcmp(out.m, GsIDMATRIX.m, sizeof(out.m)));
    CHECK(out.t[0] == 1 && out.t[1] == 2 && out.t[2] == 3);
    MATRIX composed = GsIDMATRIX;
    CHECK(RotMatrixY(1024, &composed) == &composed);
    CHECK(RotMatrixX(1024, &composed) == &composed);
    CHECK(RotMatrixZ(1024, &composed) == &composed);
    angle = (SVECTOR){1024, 1024, 1024, 0};
    RotMatrixZXY(&angle, &out);
    CHECK(!memcmp(out.m, composed.m, sizeof(out.m)));
    CHECK(out.t[0] == 1 && out.t[1] == 2 && out.t[2] == 3);
    angle = (SVECTOR){1024, 0, 0, 0};
    RotMatrix(&angle, &out);
    VECTOR input = {0, 1, 0, 0}, result;
    ApplyMatrixLV(&out, &input, &result);
    CHECK(!result.vx && !result.vy && result.vz == 1);
    angle = (SVECTOR){1024, 1024, 0, 0};
    RotMatrix(&angle, &out);
    input = (VECTOR){0, 0, 1, 0};
    ApplyMatrixLV(&out, &input, &result);
    CHECK(result.vx == 1 && !result.vy && !result.vz);
    angle = (SVECTOR){1024, 1024, 1024, 0};
    RotMatrix(&angle, &out);
    ApplyMatrixLV(&out, &input, &result);
    CHECK(result.vx == 1 && !result.vy && !result.vz);
    RotMatrixZXY(&angle, &out);
    ApplyMatrixLV(&out, &input, &result);
    CHECK(!result.vx && result.vy == 1 && !result.vz);
    CHECK(out.t[0] == 1 && out.t[1] == 2 && out.t[2] == 3);
    return 0;
}
