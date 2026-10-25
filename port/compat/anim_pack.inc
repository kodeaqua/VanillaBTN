#include "compat/asset.h"
/* Byte access retains the variable-size object's tail without indexing beyond
 * the scalar count member. The original caller selects a valid table word. */
#define ANIM_PACK_START(toc, k) \
    hm_asset_s32le((const u8 *)(toc) + 4 * ((intptr_t)(k) + 1))
