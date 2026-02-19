#include <SDL3/SDL.h>
#include <SDL3/SDL_joystick.h>

void test_joystick() {
    SDL_Joystick *joy = SDL_OpenJoystick(0);
    const char *name  = SDL_GetJoystickName(joy);
    SDL_JoystickID id = SDL_GetJoystickID(joy);
    bool virt     = SDL_IsJoystickVirtual(0);

    int axes    = SDL_GetNumJoystickAxes(joy);
    int buttons = SDL_GetNumJoystickButtons(joy);
    int hats    = SDL_GetNumJoystickHats(joy);

    Sint16 axis = SDL_GetJoystickAxis(joy, 0);
    Uint8  btn  = SDL_GetJoystickButton(joy, 0);
    Uint8  hat  = SDL_GetJoystickHat(joy, 0);

    SDL_RumbleJoystick(joy, 0xFFFF, 0xFFFF, 500);
    SDL_UpdateJoysticks();
    SDL_CloseJoystick(joy);
}
