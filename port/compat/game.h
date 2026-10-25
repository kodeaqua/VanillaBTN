#ifndef HMBTN_COMPAT_GAME_H
#define HMBTN_COMPAT_GAME_H

/* Load host declarations before mapping SDK spellings in game translation units.
 * Do not force-include this header in the platform/runtime implementation. */
#include <sys/types.h>

#include "compat/types.h"

typedef u8 hm_psx_u_char;
typedef u16 hm_psx_u_short;
typedef u32 hm_psx_u_long;

#define u_char hm_psx_u_char
#define u_short hm_psx_u_short
#define u_long hm_psx_u_long

#endif
