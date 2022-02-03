#include "process_chain_with_utils.h"

int scegliNumeroNodo(int max)
{
    int iCoda;
    struct timespec timespecRand;
    clock_gettime(CLOCK_REALTIME, &timespecRand);
    srand(timespecRand.tv_nsec);
    iCoda = rand() % max + 1;
    return iCoda;
}

int scegliAmicoNodo(int idNodo, int *puntatoreSharedMemoryAmiciNodi)
{
    int numAmici;
    int amicoRandom;
    numAmici = puntatoreSharedMemoryAmiciNodi[(idNodo - 1) * (getSoFriendsNum() * 2 + 1)];
    amicoRandom = scegliNumeroNodo(numAmici);
    return puntatoreSharedMemoryAmiciNodi[(idNodo - 1) * (getSoFriendsNum() * 2 + 1) + amicoRandom];
}
