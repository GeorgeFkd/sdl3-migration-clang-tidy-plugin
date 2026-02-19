#include <SDL2/SDL.h>

void test_surface() {
    SDL_Surface *src = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32,
                                                       SDL_PIXELFORMAT_RGBA8888);
    SDL_Surface *dst = SDL_CreateRGBSurfaceWithFormat(0, 640, 480, 32,
                                                       SDL_PIXELFORMAT_RGBA8888);

    SDL_BlitScaled(src, nullptr, dst, nullptr);
    SDL_UpperBlit(src, nullptr, dst, nullptr);

    SDL_Rect clip;
    SDL_GetClipRect(dst, &clip);
    SDL_SetClipRect(dst, nullptr);

    SDL_FillRect(dst, nullptr, 0xFF000000);

    SDL_Surface *conv = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ARGB8888, 0);

    SDL_FreeSurface(src);
    SDL_FreeSurface(dst);
    SDL_FreeSurface(conv);
}
