#include <inline_c.h>
#include <limits.h>
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
    CHECK(SquareRoot0(0) == 0 && SquareRoot0(1) == 1);
    CHECK(SquareRoot0(15) == 3 && SquareRoot0(16) == 4);
    CHECK(SquareRoot0(INT32_MAX) == 46340);
    for (s32 i = 0; i < 46340; ++i) {
        CHECK(SquareRoot0(i * i) == i);
        CHECK(SquareRoot0((i + 1) * (i + 1) - 1) == i);
    }
    VECTOR a = {3, 4, 0, 123}, b = {0, 0, 2, 456}, out = {0, 0, 0, 789};
    CHECK(VectorNormal(&a, &out) == 25);
    CHECK(out.vx == 2457 && out.vy == 3276 && out.vz == 0 && out.pad == 789);
    a = (VECTOR){1, 1, 0, 0};
    VectorNormal(&a, &a);
    CHECK(a.vx == 2896 && a.vy == 2896);
    a = (VECTOR){0, 0, 0, 0};
    CHECK(VectorNormal(&a, &out) == 0 && !out.vx && !out.vy && !out.vz);
    SVECTOR s = {-5, 0, 0, 321};
    VectorNormalSS(&s, &s);
    CHECK(s.vx == -4096 && !s.vy && !s.vz && s.pad == 321);
    MATRIX matrix = GsIDMATRIX;
    matrix.t[0] = 99;
    SetRotMatrix(&matrix);
    a = (VECTOR){3, 4, 0, 123};
    OuterProduct0(&a, &b, &a);
    CHECK(a.vx == 8 && a.vy == -6 && !a.vz && a.pad == 123);
    CHECK(!memcmp(hm_gte.matrix[0], matrix.m, sizeof(matrix.m)));
    /* Output may alias exactly three translation words, not a full VECTOR. */
    a = (VECTOR){100000, -250000, 42, 123};
    struct {
        MATRIX m;
        s32 sentinel;
    } guarded = {matrix, 0x12345678};
    CHECK(ApplyMatrixLV(&guarded.m, &a, (VECTOR *)guarded.m.t) == (VECTOR *)guarded.m.t);
    CHECK(guarded.m.t[0] == a.vx && guarded.m.t[1] == a.vy && guarded.m.t[2] == a.vz);
    CHECK(guarded.sentinel == 0x12345678);
    matrix.m[0][0] = -2048;
    a.vx = 3;
    ApplyMatrixLV(&matrix, &a, &a);
    CHECK(a.vx == -2 && a.pad == 123);
    return 0;
}
