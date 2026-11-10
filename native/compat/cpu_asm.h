#ifndef VANILLABTN_COMPAT_CPU_ASM_H
#define VANILLABTN_COMPAT_CPU_ASM_H

#include <stdlib.h>

/* Scratchpad stack switching is a PS1 1 KB d-cache optimization; on Windows
 * the host call stack is already cached. */
#define SCRATCHPAD_STACK_ENTER() ((void)0)
#define SCRATCHPAD_STACK_LEAVE() ((void)0)

/* PS1 return addresses are debugger metadata, not game state. A 32-bit MIPS
 * address has no meaning in the native process; do not truncate host pointers
 * or evaluate the discarded diagnostic destination. */
#define SAVE_RETURN_ADDRESS(where) ((void)0)

/* Original fatal loops invoke pollhost() inside host_poll() to wait for a PS1
 * debugger. Native unsupported/invalid paths stop instead of hanging. */
#define pollhost() do { abort(); return 0; } while (0)

#endif
