#include <libgpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    CHECK(FntPrint(NULL) == -1);

    s32 len1 = FntPrint("Test: %s %d\n", "val", 42);
    CHECK(len1 == (s32)strlen("Test: val 42\n"));

    s32 len2 = FntPrint("Error\n\0" "0");
    CHECK(len2 == (s32)strlen("Error\n"));

    /* Test long output triggering dynamic buffer allocation (> 2048 bytes) */
    char large_input[3000];
    memset(large_input, 'A', sizeof(large_input) - 1);
    large_input[sizeof(large_input) - 1] = '\0';
    s32 len3 = FntPrint("%s", large_input);
    CHECK(len3 == (s32)sizeof(large_input) - 1);

    puts("FntPrint test passed");
    return 0;
}
