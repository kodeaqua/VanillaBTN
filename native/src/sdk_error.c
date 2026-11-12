#include <libapi.h>
#include <stdlib.h>
#include "sdk_error.h"

#ifdef _DEBUG
static void (*error_hook)(char type, long code);

void sdk_error_set_hook(void (*hook)(char type, long code)) {
    error_hook = hook;
}
#endif

void SystemError(char type, long errorcode) {
#ifdef _DEBUG
    if (error_hook) error_hook(type, errorcode);
#else
    (void)type;
    (void)errorcode;
#endif
    abort();
}

