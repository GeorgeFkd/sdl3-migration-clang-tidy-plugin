#include <SDL2/SDL.h>

void test_audio() {
    SDL_AudioStream *stream = SDL_NewAudioStream(AUDIO_S16, 1, 44100,
                                                  AUDIO_F32, 2, 48000);
    SDL_AudioStreamPut(stream, nullptr, 0);
    SDL_AudioStreamGet(stream, nullptr, 0);
    int avail = SDL_AudioStreamAvailable(stream);
    SDL_AudioStreamFlush(stream);
    SDL_AudioStreamClear(stream);
    SDL_FreeAudioStream(stream);

    SDL_AudioSpec spec;
    Uint8 *buf = nullptr;
    Uint32 len = 0;
    SDL_LoadWAV_RW(SDL_RWFromFile("sound.wav", "rb"), 1, &spec, &buf, &len);
    SDL_FreeWAV(buf);

    SDL_MixAudioFormat(nullptr, nullptr, AUDIO_S16, 0, SDL_MIX_MAXVOLUME);
}
