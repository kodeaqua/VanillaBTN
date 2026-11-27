#ifndef VANILLABTN_GPU_PRIMITIVES_H
#define VANILLABTN_GPU_PRIMITIVES_H

#include <psyz/types.h>

int vanillabtn_gpu_push_prim(u_long *packet, int available_words);
int Draw_PushPrim(u_long *packet, int available_words);

#endif
