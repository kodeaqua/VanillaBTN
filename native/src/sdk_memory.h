#ifndef VANILLABTN_SDK_MEMORY_H
#define VANILLABTN_SDK_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#define SDK_RAM_SIZE 0x200000u
extern unsigned char sdk_main_ram[SDK_RAM_SIZE];

/* Translate serialized main-RAM addresses, including cached/uncached aliases.
 * This does not execute imported MIPS code or interpret it as a host pointer. */
void *sdk_memory_resolve(uint32_t address, size_t size);
void sdk_memory_clear(void);

#endif
