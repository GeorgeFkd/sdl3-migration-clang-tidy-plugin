#include <SDL2/SDL.h>
#include <SDL2/SDL_joystick.h>

int main() {
    SDL_Joystick *joy = SDL_JoystickOpen(0);
    const char *name  = SDL_JoystickName(joy);
    SDL_JoystickID id = SDL_JoystickInstanceID(joy);
    SDL_bool virt     = SDL_JoystickIsVirtual(0);

    int axes    = SDL_JoystickNumAxes(joy);
    int buttons = SDL_JoystickNumButtons(joy);
    int hats    = SDL_JoystickNumHats(joy);

    Sint16 axis = SDL_JoystickGetAxis(joy, 0);
    Uint8  btn  = SDL_JoystickGetButton(joy, 0);
    Uint8  hat  = SDL_JoystickGetHat(joy, 0);

    SDL_JoystickRumble(joy, 0xFFFF, 0xFFFF, 500);
    SDL_JoystickUpdate();
    SDL_JoystickClose(joy);
}
