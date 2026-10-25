#ifndef HMBTN_COMPAT_CPU_H
#define HMBTN_COMPAT_CPU_H

#include "compat/types.h"
#include "psx/error.h"

extern u8 hm_scratchpad[1024];
#define SCRATCHPAD ((uintptr_t)hm_scratchpad)
#define SCRATCHPAD_SIZE 0x400
#define SCRATCHPAD_STACK_TOP (SCRATCHPAD + SCRATCHPAD_SIZE - 4)

/* Native calls keep their normal ABI stack. No game data uses the saved PS1 SP. */
#define SCRATCHPAD_STACK_ENTER() do { } while (0)
#define SCRATCHPAD_STACK_LEAVE() do { } while (0)
/* The source stores this only as a diagnostic word, including in save metadata.
 * Use a deterministic PORT marker instead of truncating a host return address. */
#define SAVE_RETURN_ADDRESS(where) (*(where) = UINT32_C(0x504F5254))
#define pollhost() hm_runtime_error("host_poll", "game requested a debugger stop")

#endif
