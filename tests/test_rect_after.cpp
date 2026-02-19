#include <SDL3/SDL.h>

void test_rect() {
    SDL_Rect a = {0, 0, 100, 100};
    SDL_Rect b = {50, 50, 100, 100};
    SDL_Rect result;

    bool eq   = SDL_RectsEqual(&a, &b);
    bool isec = SDL_HasRectIntersection(&a, &b);
    SDL_GetRectIntersection(&a, &b, &result);
    SDL_GetRectUnion(&a, &b, &result);

    SDL_Point pts[4] = {{0,0},{10,10},{20,5},{5,15}};
    SDL_GetRectEnclosingPoints(pts, 4, nullptr, &result);

    SDL_FRect fa = {0.f, 0.f, 100.f, 100.f};
    SDL_FRect fb = {50.f, 50.f, 100.f, 100.f};
    SDL_FRect fresult;

    bool feq   = SDL_RectsEqualFloat(&fa, &fb);
    bool fisec = SDL_HasRectIntersectionFloat(&fa, &fb);
    bool fempty = SDL_RectEmptyFloat(&fa);
    SDL_GetRectIntersectionFloat(&fa, &fb, &fresult);
    SDL_GetRectUnionFloat(&fa, &fb, &fresult);
}
