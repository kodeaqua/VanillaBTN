#include <windows.h>

int main(void) {
    HMODULE module = GetModuleHandle(NULL);
    if (!module) return 1;

    /* Verify application icon (group icon ID 1) is present in PE resources */
    HRSRC group_icon = FindResource(module, MAKEINTRESOURCE(1), RT_GROUP_ICON);
    if (!group_icon) return 2;

    /* Verify version information is present in PE resources */
    HRSRC version_info = FindResource(module, MAKEINTRESOURCE(1), RT_VERSION);
    if (!version_info) return 3;

    return 0;
}
