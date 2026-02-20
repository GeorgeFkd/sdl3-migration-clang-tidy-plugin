#include <SDL3/SDL.h>

int main() {
    SDL_Mutex *mutex = SDL_CreateMutex();
    SDL_LockMutex(mutex);
    SDL_UnlockMutex(mutex);
    SDL_DestroyMutex(mutex);

    SDL_Condition *cond = SDL_CreateCondition();
    SDL_SignalCondition(cond);
    SDL_BroadcastCondition(cond);
    SDL_WaitCondition(cond, mutex);
    SDL_WaitConditionTimeout(cond, mutex, 1000);
    SDL_DestroyCondition(cond);

    SDL_Semaphore *sem = SDL_CreateSemaphore(1);
    SDL_WaitSemaphore(sem);
    SDL_TryWaitSemaphore(sem);
    SDL_WaitSemaphoreTimeout(sem, 1000);
    SDL_SignalSemaphore(sem);
    Uint32 val = SDL_GetSemaphoreValue(sem);
    SDL_DestroySemaphore(sem);
}
