#define menu_run_state hm_test_menu_run
#include "../../src/overlay/23_GF_MG4/mg4.c"
#undef menu_run_state
#include <stdio.h>
_Static_assert(_Generic((Fn)0, void (*)(void): 1, default: 0), "native state callback ABI");
static int menus, handlers, bad;
s32 hm_test_menu_run(void) {
    ++menus;
    return 0;
}
static void hm_test_handler(void) {
    if (menus != handlers + 1 || mg4_state.state != handlers)
        bad = 1;
    ++handlers;
}
int main(void) {
    for (int i = 0; i < 7; ++i) {
        mg4_state_handlers[i] = hm_test_handler;
        mg4_state.state = i;
        mg4_state_tick();
    }
    if (bad || menus != 7 || handlers != 7)
        return 1;
    return 0;
}
