#include <libgte.h>

#include <string.h>

int main(int argc, char **argv) {
    VECTOR input = {0};
    VECTOR output;
    if (argc != 2) {
        return 2;
    }
    if (strcmp(argv[1], "SquareRoot0") == 0) {
        return SquareRoot0(0);
    }
    if (strcmp(argv[1], "VectorNormal") == 0) {
        return VectorNormal(&input, &output);
    }
    return 2;
}
