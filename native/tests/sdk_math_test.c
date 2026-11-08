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
    return 0;
}
