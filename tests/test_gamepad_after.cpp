#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>

int main() {
    if (!SDL_IsGamepad(0))
        return 0;

    SDL_Gamepad *gc = SDL_OpenGamepad(0);
    const char *name = SDL_GetGamepadName(gc);
    bool attached = SDL_GamepadConnected(gc);

    Sint16 axis = SDL_GetGamepadAxis(gc, SDL_GAMEPAD_AXIS_LEFTX);
    Uint8 btn = SDL_GetGamepadButton(gc, SDL_GAMEPAD_BUTTON_SOUTH);

    SDL_RumbleGamepad(gc, 0xFFFF, 0xFFFF, 500);
    SDL_SetGamepadLED(gc, 255, 0, 0);

    SDL_CloseGamepad(gc);

    SDL_AddGamepadMapping("...");
    SDL_UpdateGamepads();
}
