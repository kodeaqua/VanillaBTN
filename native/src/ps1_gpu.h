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

#include "ps1_ot.h"
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

/*
 * Submit all packets in an ordering table to the GPU dispatcher.
 *
 * Each packet_id visited in the OT is a word index into `packet_buffer`.
 * The word at `packet_buffer[packet_id]` is the PS1 primitive tag:
 * bits 31-24 hold the packet length in 32-bit words (excluding the tag).
 * The GP0 command words follow at `packet_buffer[packet_id + 1]` through
 * `packet_buffer[packet_id + len]`.
 *
 * Returns false if arguments are NULL or a packet extends beyond buffer_words.
 */
bool ps1_gpu_submit_ot(Ps1Video *video, Ps1DrawState *state,
                       const Ps1Ot *ot,
                       const uint32_t *packet_buffer, size_t buffer_words);

/*
 * Submit an ordered sequence of ordering tables for a complete frame.
 *
 * Traverses tables in `ots[0]` .. `ots[ot_count - 1]`, dispatching all
 * packets through `ps1_gpu_submit_ot`. Any NULL table pointer in the array
 * is skipped (matching the game's optional OT tags).
 */
bool ps1_gpu_submit_frame(Ps1Video *video, Ps1DrawState *state,
                          const Ps1Ot * const *ots, size_t ot_count,
                          const uint32_t *packet_buffer, size_t buffer_words);

#endif /* VANILLABTN_PS1_GPU_H */
