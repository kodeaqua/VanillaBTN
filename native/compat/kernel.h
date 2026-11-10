#ifndef VANILLABTN_COMPAT_KERNEL_H
#define VANILLABTN_COMPAT_KERNEL_H

#include "types.h"

struct DIRENTRY {
    char name[20];
    long attr;
    long size;
    struct DIRENTRY *next;
    long head;
    char system[4];
};

#endif /* VANILLABTN_COMPAT_KERNEL_H */
