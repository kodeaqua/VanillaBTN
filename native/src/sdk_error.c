#include <libapi.h>
#include <stdlib.h>

void SystemError(char type, long errorcode) {
    (void)type;
    (void)errorcode;
    abort();
}

