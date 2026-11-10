#include "host_gamepad.h"
#include <stdio.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); return 1; } } while (0)

int main(void) {
    CHECK(SDL_Init(SDL_INIT_GAMEPAD));
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    CHECK(id != 0);
    SDL_Gamepad *gamepad = SDL_OpenGamepad(id);
    CHECK(gamepad);
    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gamepad);
    CHECK(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true));
    CHECK(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, 12345));
    CHECK(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767));
    SDL_UpdateJoysticks();
    HostPadSnapshot restored = host_gamepad_snapshot(gamepad);
    CHECK(restored.buttons == PS1_PAD_CROSS);
    CHECK(restored.x == 12345);
    CHECK(restored.left_trigger >= 16384);
    CHECK(ps1_input_trigger_buttons(restored.left_trigger, restored.right_trigger) == PS1_PAD_L2);
    CHECK(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false));
    CHECK(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, 0));
    SDL_UpdateJoysticks();
    restored = host_gamepad_snapshot(gamepad);
    CHECK(restored.buttons == 0 && restored.x == 0);
    SDL_CloseGamepad(gamepad);
    CHECK(SDL_DetachVirtualJoystick(id));
    SDL_Quit();
    return 0;
}
