#include <SDL2/SDL.h>
#include <SDL2/SDL_haptic.h>

int main() {
    SDL_Haptic *haptic = SDL_HapticOpen(0);

    SDL_HapticRumbleInit(haptic);
    SDL_HapticRumblePlay(haptic, 0.5f, 1000);
    SDL_HapticRumbleStop(haptic);

    int axes = SDL_HapticNumAxes(haptic);
    int effects = SDL_HapticNumEffects(haptic);
    int playing = SDL_HapticNumEffectsPlaying(haptic);
    unsigned cap = SDL_HapticQuery(haptic);

    SDL_HapticSetGain(haptic, 50);
    SDL_HapticSetAutocenter(haptic, 0);
    SDL_HapticPause(haptic);
    SDL_HapticUnpause(haptic);
    SDL_HapticStopAll(haptic);
    SDL_HapticClose(haptic);

    SDL_Joystick *joy = SDL_JoystickOpen(0);
    if (SDL_JoystickIsHaptic(joy)) {
        SDL_Haptic *jh = SDL_HapticOpenFromJoystick(joy);
        SDL_HapticClose(jh);
    }
}
