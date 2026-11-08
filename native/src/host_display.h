#ifndef VANILLABTN_HOST_DISPLAY_H
#define VANILLABTN_HOST_DISPLAY_H

#include "ps1_video.h"
#include <SDL3/SDL.h>

/* Present the SDK-selected VRAM rectangle, retaining original internal size.
 * Replaces the owned streaming texture when the game changes display size. */
bool host_present_frame(const Ps1Video *video, SDL_Renderer *renderer, SDL_Texture **texture);

#endif
