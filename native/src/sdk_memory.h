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

/* Address a logical field when its base is either a native runtime object or
 * a PS1 RAM address. Original null data accesses refer to physical RAM zero.
 * The explicit PS1 offset preserves wire layout when host pointer sizes grow.
 * Native objects are borrowed spans whose bounds remain the caller's contract. */
void *sdk_memory_field(void *base, size_t native_offset, uint32_t ps1_offset,
                       size_t size);

#endif
