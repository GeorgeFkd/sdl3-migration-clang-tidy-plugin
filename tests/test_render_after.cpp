#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>

int main() {
    SDL_Window *window = SDL_CreateWindow("Hello World", 640, 480, 0);
    SDL_Renderer *renderer = SDL_GetRenderer(window);

    SDL_RenderLine(renderer, 0, 0, 100, 100);
    SDL_RenderPoint(renderer, 50, 50);
    SDL_RenderRect(renderer, nullptr);
    SDL_RenderFillRect(renderer, nullptr);

    SDL_Texture *tex =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 1024, 768);
    SDL_RenderTexture(renderer, tex, nullptr, nullptr);
    SDL_RenderTextureRotated(renderer, tex, nullptr, nullptr, 0.0, nullptr, SDL_FLIP_NONE);

    // State management
    SDL_SetRenderViewport(renderer, nullptr);
    SDL_GetRenderViewport(renderer, nullptr);
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    SDL_GetRenderScale(renderer, nullptr, nullptr);
    SDL_SetRenderLogicalPresentation(renderer, 1920, 1080, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderClipRect(renderer, nullptr);
    SDL_GetRenderClipRect(renderer, nullptr);
    SDL_RenderClipEnabled(renderer);
    SDL_SetRenderVSync(renderer, 1);
    SDL_FlushRenderer(renderer);

    int w, h;
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
}
