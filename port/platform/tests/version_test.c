#include "platform/version.h"
#include "platform/crash_handler.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); return 1; } } while (0)

int main(void) {
    hm_crash_handler_install();

    const char *ver = hm_version_string();
    CHECK(ver != NULL);
    CHECK(!strcmp(ver, "0.9.0"));

    const char *title = hm_version_window_title();
    CHECK(title != NULL);
    CHECK(strstr(title, "VanillaBTN") != NULL);
    CHECK(strstr(title, "v0.9.0") != NULL);

    return 0;
}
