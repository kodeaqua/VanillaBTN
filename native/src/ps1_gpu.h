#ifndef VANILLABTN_PS1_GPU_H
#define VANILLABTN_PS1_GPU_H

/*
 * ps1_gpu: GP0 command dispatcher for the native Windows target.
 *
 * The PS1 GPU receives drawing commands through its GP0 port as a stream of
 * 32-bit words.  The first word of each command encodes the command code in
 * bits 31-24, the colour in bits 23-0, and the word-length of the whole packet
 * in the OT tag's high byte.  The dispatcher here reads that same packet layout
 * and routes each recognised command to the ps1_video software renderer.
 *
 * Supported GP0 codes (see PSX-SPX GPU reference):
 *   0x02        Quick fill (ignores draw state)
 *   0x60/0x62   Variable-size untextured tile, opaque / semi-transparent
 *   0x68/0x6A   Single-pixel tile (1x1), opaque / semi-transparent
 *   0x64/0x65/0x66/0x67  Variable-size textured sprite, raw / modulated
 *   0x74/0x75/0x76/0x77  16x16 textured sprite
 *   0x7C/0x7D/0x7E/0x7F  8x8 textured sprite
 *   0xE1        Draw mode / texture page setting
 *   0xE3        Set draw area top-left
 *   0xE4        Set draw area bottom-right
 *   0xE5        Set draw offset
 *   0xE6        Mask bit setting
 *
 * Unknown codes are silently skipped so an OT that includes unimplemented
 * primitives does not abort the whole frame.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ps1_video.h"

/*
 * Submit one GP0 packet stored as an array of 32-bit words.
 *
 * `words`  points to the first word of the packet (the command/colour word).
 * `count`  is the number of words in the packet (must be >= 1).
 * `state`  is the current draw state; GP0(E1/E3/E4/E5/E6) commands update it.
 * `video`  is the VRAM target; drawing commands write into it.
 *
 * Returns false only when the pointer arguments are NULL or count is 0.
 * An unrecognised command code returns true (silently ignored).
 */
bool ps1_gpu_exec(Ps1Video *video, Ps1DrawState *state,
                  const uint32_t *words, size_t count);

#endif /* VANILLABTN_PS1_GPU_H */
