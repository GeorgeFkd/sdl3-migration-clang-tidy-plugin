#include <SDL2/SDL.h>

int main(){
    SDL_mutex *mutex = SDL_CreateMutex();
    SDL_LockMutex(mutex);
    SDL_UnlockMutex(mutex);
    SDL_DestroyMutex(mutex);

    SDL_cond *cond = SDL_CreateCond();
    SDL_CondSignal(cond);
    SDL_CondBroadcast(cond);
    SDL_CondWait(cond, mutex);
    SDL_CondWaitTimeout(cond, mutex, 1000);
    SDL_DestroyCond(cond);

    SDL_sem *sem = SDL_CreateSemaphore(1);
    SDL_SemWait(sem);
    SDL_SemTryWait(sem);
    SDL_SemWaitTimeout(sem, 1000);
    SDL_SemPost(sem);
    Uint32 val = SDL_SemValue(sem);
    SDL_DestroySemaphore(sem);
}
