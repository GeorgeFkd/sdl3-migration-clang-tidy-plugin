#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
int main() {
  SDL_Window *window = SDL_CreateWindow("Hello World", 0, 0, 640, 480, 0);
  SDL_Renderer *renderer = SDL_GetRenderer(window);

  SDL_RenderDrawLine(renderer, 0, 0, 100, 100);
  SDL_RenderDrawPoint(renderer, 50, 50);
  SDL_RenderDrawRect(renderer, nullptr);
  SDL_RenderFillRectF(renderer, nullptr);

  SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_TARGET, 1024, 768);
  SDL_RenderCopy(renderer, tex, nullptr, nullptr);
  SDL_RenderCopyEx(renderer, tex, nullptr, nullptr, 0.0, nullptr,
                   SDL_FLIP_NONE);

  SDL_RenderSetViewport(renderer, nullptr);
  SDL_RenderGetViewport(renderer, nullptr);
  SDL_RenderSetScale(renderer, 1.0f, 1.0f);
  SDL_RenderGetScale(renderer, nullptr, nullptr);
  SDL_RenderSetLogicalSize(renderer, 1920, 1080);
  SDL_RenderSetClipRect(renderer, nullptr);
  SDL_RenderGetClipRect(renderer, nullptr);
  SDL_RenderIsClipEnabled(renderer);
  SDL_RenderSetVSync(renderer, 1);
  SDL_RenderFlush(renderer);

  int w, h;
  SDL_GetRendererOutputSize(renderer, &w, &h);
}
