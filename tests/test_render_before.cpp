#include <SDL2/SDL.h>

void test_render(SDL_Renderer *renderer, SDL_Texture *tex) {
    // Drawing primitives
    SDL_RenderDrawLine(renderer, 0, 0, 100, 100);
    SDL_RenderDrawPoint(renderer, 50, 50);
    SDL_RenderDrawRect(renderer, nullptr);
    SDL_RenderFillRectF(renderer, nullptr);

    // Texture rendering
    SDL_RenderCopy(renderer, tex, nullptr, nullptr);
    SDL_RenderCopyEx(renderer, tex, nullptr, nullptr, 0.0, nullptr, SDL_FLIP_NONE);

    // State management
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
