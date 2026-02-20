#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
int main() {
  SDL_AudioSpec srcspec = {SDL_AUDIO_S16LE, 1, 44100};
  SDL_AudioSpec dstspec = {SDL_AUDIO_F32LE, 2, 48000};
  SDL_AudioStream *stream = SDL_CreateAudioStream(&srcspec, &dstspec);
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

  SDL_MixAudio(nullptr, nullptr, SDL_AUDIO_S16LE, 0, 1.0f);
  SDL_MixAudio(nullptr, nullptr, SDL_AUDIO_S16LE, 0, (float)64 / 128);
}
