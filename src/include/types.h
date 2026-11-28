#ifndef TYPES_H
#define TYPES_H

/* Primitive integer typedefs shared across the source tree. */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

/* PSY-Q addresses ordering tables as words. PsyZ defines OT_TYPE in libgpu.h. */
#ifndef __psyz
#define OT_TYPE u_long
#endif

#endif /* TYPES_H */
