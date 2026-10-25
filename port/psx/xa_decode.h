#ifndef HMBTN_PSX_XA_DECODE_H
#define HMBTN_PSX_XA_DECODE_H
#include "psx/spu_decode.h"
enum { HM_XA_FRAMES = 2352 };
typedef struct {
    hm_adpcm_history history[2];
    s16 ring[2][32];
    u32 position, samples;
} hm_xa_decoder;
void hm_xa_decoder_reset(hm_xa_decoder *decoder);
/* Source disc uses only 37.8 kHz stereo, 4-bit XA. Other coding fails explicitly
 * at the transport layer. Output contains HM_XA_FRAMES stereo frames at 44.1 kHz. */
int hm_xa_decode_sector(hm_xa_decoder *decoder, const u8 raw[2352], s16 *stereo);
#endif
