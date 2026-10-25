#include "psx/unimplemented.h"

#include <stdio.h>
#include <stdlib.h>

_Noreturn void hm_unimplemented(const char *function) {
    fprintf(stderr, "Unimplemented PSX API: %s\n", function);
    fflush(stderr);
    abort();
}
