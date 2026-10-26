#include "psx/unimplemented.h"
#include "psx/error.h"

#include <stdio.h>
#include <stdlib.h>

_Noreturn void hm_unimplemented(const char *function) {
    fprintf(stderr, "Unimplemented PSX API: %s\n", function);
    fflush(stderr);
    abort();
}

_Noreturn void hm_runtime_error(const char *function, const char *detail) {
    fprintf(stderr, "PSX runtime error: %s: %s\n", function, detail);
    fflush(stderr);
    abort();
}
