#ifndef SDK_MCRD_H
#define SDK_MCRD_H

/* PsyZ's kernel.h defines struct DIRENTRY without a guard macro, so libmcrd.h
 * collides and redefines O_RDONLY/O_WRONLY. Preserve kernel.h's struct DIRENTRY. */
#ifdef __psyz
#include <kernel.h>
#define DIRENTRY DIRENTRY
#ifdef O_RDONLY
#pragma push_macro("O_RDONLY")
#undef O_RDONLY
#define SDK_MCRD_RESTORE_O_RDONLY
#endif
#ifdef O_WRONLY
#pragma push_macro("O_WRONLY")
#undef O_WRONLY
#define SDK_MCRD_RESTORE_O_WRONLY
#endif
#include <libmcrd.h>
#undef DIRENTRY
#ifdef SDK_MCRD_RESTORE_O_RDONLY
#pragma pop_macro("O_RDONLY")
#undef SDK_MCRD_RESTORE_O_RDONLY
#endif
#ifdef SDK_MCRD_RESTORE_O_WRONLY
#pragma pop_macro("O_WRONLY")
#undef SDK_MCRD_RESTORE_O_WRONLY
#endif
#else
#include <libmcrd.h>
#endif

#endif /* SDK_MCRD_H */
