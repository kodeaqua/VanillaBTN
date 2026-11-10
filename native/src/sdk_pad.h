#ifndef VANILLABTN_SDK_PAD_H
#define VANILLABTN_SDK_PAD_H

#include <stdbool.h>
#include <stdint.h>

/* Samples contain the game's PS1 button bits and raw 0..255 stick positions.
 * Original pad.c owns interpretation, replay, repeats and vibration requests. */
void sdk_pad_sample(unsigned slot, bool connected, bool analog, uint16_t buttons,
                     uint8_t left_x, uint8_t left_y);
void sdk_pad_rumble(void (*send)(unsigned slot, uint8_t small, uint8_t large, void *), void *context);

#endif
