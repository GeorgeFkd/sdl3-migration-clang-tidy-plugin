#include <SDL2/SDL.h>
#include <SDL2/SDL_gamecontroller.h>

int main() {
  if (!SDL_IsGameController(0))
    return 0;

  SDL_GameController *gc = SDL_GameControllerOpen(0);
  const char *name = SDL_GameControllerName(gc);
  SDL_bool attached = SDL_GameControllerGetAttached(gc);

  Sint16 axis = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
  Uint8 btn = SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_A);

  SDL_GameControllerRumble(gc, 0xFFFF, 0xFFFF, 500);
  SDL_GameControllerSetLED(gc, 255, 0, 0);

  SDL_GameControllerClose(gc);

  SDL_GameControllerAddMapping("...");
  SDL_GameControllerUpdate();
}
