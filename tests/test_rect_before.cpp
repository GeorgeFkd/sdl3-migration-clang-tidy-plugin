#include <SDL2/SDL.h>

int main(){
    SDL_Rect a = {0, 0, 100, 100};
    SDL_Rect b = {50, 50, 100, 100};
    SDL_Rect result;

    SDL_bool eq   = SDL_RectEquals(&a, &b);
    SDL_bool isec = SDL_HasIntersection(&a, &b);
    SDL_IntersectRect(&a, &b, &result);
    SDL_UnionRect(&a, &b, &result);

    SDL_Point pts[4] = {{0,0},{10,10},{20,5},{5,15}};
    SDL_EnclosePoints(pts, 4, nullptr, &result);

    SDL_FRect fa = {0.f, 0.f, 100.f, 100.f};
    SDL_FRect fb = {50.f, 50.f, 100.f, 100.f};
    SDL_FRect fresult;

    SDL_bool feq   = SDL_FRectEquals(&fa, &fb);
    SDL_bool fisec = SDL_HasIntersectionF(&fa, &fb);
    SDL_bool fempty = SDL_FRectEmpty(&fa);
    SDL_IntersectFRect(&fa, &fb, &fresult);
    SDL_UnionFRect(&fa, &fb, &fresult);
}
