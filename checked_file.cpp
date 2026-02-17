#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <iostream>

int main() {
    // Initialize SDL audio subsystem
    if (SDL_AudioInit("alsa") < 0) {
        std::cerr << "Failed to initialize audio: " << SDL_GetError() << "\n";
        return 1;
    }
    
    std::cout << "Audio initialized successfully\n";
    
    // Do some audio work here
    int numPlaybackAudioDevices = SDL_GetNumAudioDevices(0);
    std::cout << "Number of audio devices: " << numPlaybackAudioDevices << "\n";
    int numDevices = SDL_GetNumAudioDevices(1);

    SDL_PauseAudioDevice(1, 1);
    SDL_PauseAudioDevice(2,0);
    SDL_GetAudioDeviceStatus(3);
    
    // Clean up audio subsystem
    SDL_AudioQuit();
    
    std::cout << "Audio quit successfully\n";
    
    return 0;
}
