#include <SDL3/SDL.h>

void test_audio() {
    SDL_AudioSpec srcspec = {SDL_AUDIO_S16,1,44100};
    SDL_AudioSpec dstspec = {SDL_AUDIO_F32,2,48000};
    SDL_AudioStream *stream = SDL_CreateAudioStream(&srcspec,&dstspec);
    SDL_PutAudioStreamData(stream, nullptr, 0);
    SDL_GetAudioStreamData(stream, nullptr, 0);
    int avail = SDL_GetAudioStreamAvailable(stream);
    SDL_FlushAudioStream(stream);
    SDL_ClearAudioStream(stream);
    SDL_DestroyAudioStream(stream);

    SDL_AudioSpec spec;
    Uint8 *buf = nullptr;
    Uint32 len = 0;
    SDL_LoadWAV_IO(SDL_IOFromFile("sound.wav", "rb"), 1, &spec, &buf, &len);
    SDL_free(buf);

    SDL_MixAudio(nullptr, nullptr, SDL_AUDIO_S16, 0, 0.0f);
}
