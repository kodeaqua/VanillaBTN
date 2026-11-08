#ifndef VANILLABTN_COMPAT_CPU_ASM_H
#define VANILLABTN_COMPAT_CPU_ASM_H

/* PS1 return addresses are debugger metadata, not game state. A 32-bit MIPS
 * address has no meaning in the native process; do not truncate host pointers
 * or evaluate the discarded diagnostic destination. */
#define SAVE_RETURN_ADDRESS(where) ((void)0)

#endif
