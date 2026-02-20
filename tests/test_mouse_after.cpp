#include <SDL3/SDL.h>

int main() {
    SDL_Cursor *cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    SDL_SetCursor(cursor);
    SDL_DestroyCursor(cursor);

    SDL_Cursor *wait = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_PROGRESS);
    SDL_DestroyCursor(wait);

    SDL_Cursor *resize = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
    SDL_DestroyCursor(resize);
}
