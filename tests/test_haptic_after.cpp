#include <SDL3/SDL.h>
#include <SDL3/SDL_haptic.h>

int main() {
    SDL_Haptic *haptic = SDL_OpenHaptic(0);

    SDL_InitHapticRumble(haptic);
    SDL_PlayHapticRumble(haptic, 0.5f, 1000);
    SDL_StopHapticRumble(haptic);

    int axes = SDL_GetNumHapticAxes(haptic);
    int effects = SDL_GetMaxHapticEffects(haptic);
    int playing = SDL_GetMaxHapticEffectsPlaying(haptic);
    unsigned cap = SDL_GetHapticFeatures(haptic);

    SDL_SetHapticGain(haptic, 50);
    SDL_SetHapticAutocenter(haptic, 0);
    SDL_PauseHaptic(haptic);
    SDL_ResumeHaptic(haptic);
    SDL_StopHapticEffects(haptic);
    SDL_CloseHaptic(haptic);

    SDL_Joystick *joy = SDL_OpenJoystick(0);
    if (SDL_IsJoystickHaptic(joy)) {
        SDL_Haptic *jh = SDL_OpenHapticFromJoystick(joy);
        SDL_CloseHaptic(jh);
    }
}
