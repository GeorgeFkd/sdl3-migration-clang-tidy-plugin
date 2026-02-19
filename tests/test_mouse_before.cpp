#include <SDL2/SDL.h>

void test_mouse() {
    SDL_Cursor *cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    SDL_SetCursor(cursor);
    SDL_FreeCursor(cursor);

    SDL_Cursor *wait = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAITARROW);
    SDL_FreeCursor(wait);

    SDL_Cursor *resize = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    SDL_FreeCursor(resize);
}
