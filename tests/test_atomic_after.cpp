#include <SDL3/SDL.h>

int main() {
  SDL_SpinLock lock = 0;
  SDL_LockSpinlock(&lock);
  SDL_UnlockSpinlock(&lock);
  SDL_TryLockSpinlock(&lock);

  SDL_AtomicInt a = {0};
  SDL_SetAtomicInt(&a, 42);
  SDL_GetAtomicInt(&a);
  SDL_AddAtomicInt(&a, 1);
  SDL_CompareAndSwapAtomicInt(&a, 0, 1);
}
