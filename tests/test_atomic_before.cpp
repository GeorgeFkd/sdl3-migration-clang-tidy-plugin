#include <SDL2/SDL.h>

void test_atomic() {
    SDL_SpinLock lock = 0;
    SDL_AtomicLock(&lock);
    SDL_AtomicUnlock(&lock);
    SDL_AtomicTryLock(&lock);

    SDL_atomic_t a = {0};
    SDL_AtomicSet(&a, 42);
    SDL_AtomicGet(&a);
    SDL_AtomicAdd(&a, 1);
    SDL_AtomicCAS(&a, 0, 1);
}
