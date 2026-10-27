#include "compat/async_wait.h"
#include "sound_request.h"
#include <libetc.h>
/* SDK interrupt-driven loaders need deterministic hardware progress while the
 * game is inside an original blocking wait, including overnight transitions. */
void hm_wait_sound_fader(void) {
    while (!sound_fader_ready()) {
        sound_load_tick();
        VSync(0);
    }
}
