#include <libgte.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--reference") == 0) {
        for (int x = 0; x <= 128; ++x)
            for (int y = 0; y <= 128; ++y)
                printf("%d %d %ld %d\n", x, y, ratan2(x, y), csqrt((x * x + y * y) << 12));
        return 0;
    }
    if (argc != 1) return 1;
    /* Reference SDK outputs include CORDIC quantization, not real sqrt. */
    if (csqrt(0) != 0 || csqrt(1) != 64 || csqrt(4096) != 4096
        || csqrt(8192) != 5792 || csqrt(409600) != 40962
        || csqrt(67108864) != 524321 || csqrt(134217728) != 741496) return 2;
    if (ratan2(0, 1) != 0 || ratan2(1, 0) != 1024 || ratan2(0, -1) != 2048
        || ratan2(-1, 0) != -1024 || ratan2(1, 1) != 512 || ratan2(-1, -1) != -1536) return 3;
    if (ratan2(2, 1024) != 2 || ratan2(5, 1024) != 3 || ratan2(7, 1024) != 5) return 4;
    if (rsin(0) != 0 || rsin(512) != 2896 || rsin(1024) != 4096
        || rsin(2048) != 0 || rsin(3072) != -4096 || rsin(-512) != -2896) return 5;
    if (rcos(0) != 4096 || rcos(512) != 2896 || rcos(1024) != 0
        || rcos(2048) != -4096 || rcos(3072) != 0 || rcos(-512) != 2896) return 6;
    /* csin/ccos/csincos use CORDIC quantization from LIBGTE COR_01/02/03. */
    int s_val = 0, c_val = 0;
    csincos(0, &s_val, &c_val);
    if (s_val != -56 || c_val != 4095) return 14;
    csincos(512, &s_val, &c_val);
    if (s_val != 2898 || c_val != 2892) return 15;
    if (csin(0) != -56 || csin(512) != 2898 || csin(1024) != 4094
        || csin(2048) != 56 || csin(3072) != -4094 || csin(-512) != -2898) return 16;
    if (ccos(0) != 4095 || ccos(512) != 2892 || ccos(1024) != 56
        || ccos(2048) != -4095 || ccos(3072) != -56 || ccos(-512) != 2892) return 17;
    MATRIX matrix = {0};
    VECTOR translation = {123, -456, 789, 0};
    SVECTOR angles = {1024, 1024, 0, 0};
    if (TransMatrix(&matrix, &translation) != &matrix) return 7;
    RotMatrixZXY(&angles, &matrix);
    const short expected[3][3] = {{0, 0, 4096}, {4096, 0, 0}, {0, 4096, 0}};
    if (memcmp(matrix.m, expected, sizeof(expected)) ||
        matrix.t[0] != 123 || matrix.t[1] != -456 || matrix.t[2] != 789) return 8;
    /* Numerical outputs from local reference SDK microcalls, not a second
     * implementation of the rotation formula. */
    const SVECTOR rotations[2] = {{123, 456, 789, 0}, {-200, 300, -400, 0}};
    const short reference[2][3][3] = {
        {{643, -3765, 1479}, {3106, 1419, 2260}, {-2591, 768, 3078}},
        {{2684, 2248, 2125}, {-2564, 3192, -141}, {-1735, -1237, 3498}}
    };
    for (int i = 0; i < 2; ++i) {
        angles = rotations[i];
        RotMatrixZXY(&angles, &matrix);
        if (memcmp(matrix.m, reference[i], sizeof(reference[i]))) return 9;
    }
    MATRIX camera = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {40000, -40000, 100}};
    gte_SetRotMatrix(&camera);
    gte_SetTransMatrix(&camera);
    MATRIX readback = {0};
    gte_ReadRotMatrix(&readback);
    if (memcmp(readback.m, camera.m, sizeof(camera.m)) ||
        memcmp(readback.t, camera.t, sizeof(camera.t))) return 10;
    SVECTOR zero = {0};
    gte_ldv0(&zero);
    gte_rtv0();
    long limited[3], wide[3];
    gte_stlvl(limited);
    gte_stlvnl(wide);
    if (limited[0] != 32767 || limited[1] != -32768 || limited[2] != 100 ||
        wide[0] != 40000 || wide[1] != -40000 || wide[2] != 100) return 11;
    camera.t[0] = camera.t[1] = camera.t[2] = 0;
    gte_SetTransMatrix(&camera);
    VECTOR raw = {65537, -65534, 32768, 0};
    gte_ldlv0(&raw);
    gte_rtv0();
    gte_stlvnl(wide);
    if (wide[0] != 1 || wide[1] != 2 || wide[2] != -32768) return 12;
    gte_ldlvl(&raw);
    gte_rtir();
    gte_stlvnl(wide);
    if (wide[0] != 1 || wide[1] != 2 || wide[2] != -32768) return 13;
    return 0;
}
