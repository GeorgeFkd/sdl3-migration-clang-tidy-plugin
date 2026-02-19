#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>

void test_surface() {
    SDL_Surface *src = SDL_CreateSurface(640, 480,
                                                       SDL_PIXELFORMAT_RGBA8888);
    SDL_Surface *dst = SDL_CreateSurface(640, 480,
                                                       SDL_PIXELFORMAT_RGBA8888);

    SDL_BlitSurfaceScaled(src, nullptr, dst, nullptr,SDL_SCALEMODE_LINEAR);
    SDL_BlitSurface(src, nullptr, dst, nullptr);

    SDL_Rect clip;
    SDL_GetSurfaceClipRect(dst, &clip);
    SDL_SetSurfaceClipRect(dst, nullptr);

    SDL_FillSurfaceRect(dst, nullptr, 0xFF000000);

    SDL_Surface *conv = SDL_ConvertSurface(src, SDL_PIXELFORMAT_ARGB8888);

    SDL_DestroySurface(src);
    SDL_DestroySurface(dst);
    SDL_DestroySurface(conv);
}
