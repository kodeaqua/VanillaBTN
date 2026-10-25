#include "math_util.h"

#include <stdio.h>

#define CHECK(condition)                                                                            \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    s32 value;
    s32 from[3] = {-7, 20, 100};
    s32 to[3] = {8, -20, 0};
    s32 out[3];

    CHECK(math_int_pow(7, 0) == 1);
    CHECK(math_int_pow(7, -3) == 1);
    CHECK(math_int_pow(-3, 3) == -27);
    CHECK(math_int_pow(65536, 2) == 0); /* Original 32-bit wrap semantics. */
    math_lerp_scalar(0, 4, -7, 8, &value);
    CHECK(value == -7);
    math_lerp_scalar(1, 4, -7, 8, &value);
    CHECK(value == -3); /* Integer division truncates toward zero. */
    math_lerp_scalar(9, 4, -7, 8, &value);
    CHECK(value == 8); /* Steps past the endpoint are clamped. */
    math_lerp_vec3(1, 4, from, to, out);
    CHECK(out[0] == -3 && out[1] == 10 && out[2] == 75);
    vec3_copy(out, from);
    vec3_sub(out, to);
    CHECK(out[0] == -15 && out[1] == 40 && out[2] == 100);
    return 0;
}
