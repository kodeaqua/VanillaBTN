#ifndef VANILLABTN_COMPAT_SDK_TYPES_H
#define VANILLABTN_COMPAT_SDK_TYPES_H

/* Windows uses 32-bit long on x64 too. Do not shadow CRT sys/types.h: SDL
 * and system headers need its native file/stat types. */
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

#endif
