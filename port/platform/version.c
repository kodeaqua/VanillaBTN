#include "platform/version.h"

const char *hm_version_string(void) {
    return HMBTN_VERSION_STRING;
}

const char *hm_version_window_title(void) {
    return HMBTN_TITLE_STRING " v" HMBTN_VERSION_STRING;
}
