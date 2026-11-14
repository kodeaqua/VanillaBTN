#ifndef VANILLABTN_HOST_TEXTURE_PACK_H
#define VANILLABTN_HOST_TEXTURE_PACK_H

#include "ps1_video.h"

#include <stdbool.h>

bool host_texture_pack_configure(const char *relative_directory);
void host_texture_pack_disable(void);
bool host_texture_pack_lookup(uint64_t content_key, int source_width,
    int source_height, Ps1ReplacementImage *out_image, void *userdata);

#endif
