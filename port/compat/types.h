#ifndef HMBTN_PORT_TYPES_H
#define HMBTN_PORT_TYPES_H

#include <stdint.h>

/* Match src/include/types.h's guard when force-including this header. */
#ifndef TYPES_H
#define TYPES_H
typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
#endif
typedef int64_t s64;
typedef uint64_t u64;

_Static_assert(sizeof(void *) == 8, "The native port requires a 64-bit target");
_Static_assert(sizeof(s32) == 4, "PS1 words must remain 32-bit");

#endif
