#ifndef VANILLABTN_HOST_GAMEPAD_H
#define VANILLABTN_HOST_GAMEPAD_H

#include "ps1_input.h"
#include <SDL3/SDL.h>

typedef struct HostPadSnapshot {
    uint32_t buttons;
    int16_t x, y, left_trigger, right_trigger;
} HostPadSnapshot;

static uint32_t host_gamepad_button(SDL_GamepadButton button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return PS1_PAD_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return PS1_PAD_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return PS1_PAD_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return PS1_PAD_RIGHT;
    case SDL_GAMEPAD_BUTTON_SOUTH: return PS1_PAD_CROSS;
    case SDL_GAMEPAD_BUTTON_EAST: return PS1_PAD_CIRCLE;
    case SDL_GAMEPAD_BUTTON_WEST: return PS1_PAD_SQUARE;
    case SDL_GAMEPAD_BUTTON_NORTH: return PS1_PAD_TRIANGLE;
    case SDL_GAMEPAD_BUTTON_START: return PS1_PAD_START;
    case SDL_GAMEPAD_BUTTON_BACK: return PS1_PAD_SELECT;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return PS1_PAD_L1;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return PS1_PAD_R1;
    default: return 0;
    }
}

static HostPadSnapshot host_gamepad_snapshot(SDL_Gamepad *gamepad) {
    HostPadSnapshot state = {0};
    if (!gamepad) return state;
    for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button)
        if (SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)button))
            state.buttons |= host_gamepad_button((SDL_GamepadButton)button);
    state.x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    state.y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    state.left_trigger = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    state.right_trigger = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    return state;
}

#endif
