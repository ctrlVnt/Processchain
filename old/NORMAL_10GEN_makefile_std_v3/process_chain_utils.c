#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include "process_chain_utils.h"

void attesaNonAttiva(long nsecMin, long nsecMax)
{
    long ntos;
    long diff;
    long attesa;
    long nsec;
    int sec;
    struct timespec tempoDiAttesa;
    srand(getpid());
    ntos = 1e9L;
    diff = nsecMax - nsecMin;
    if (diff == 0)
    {
        attesa = nsecMax;
    }
    else
    {
        attesa = rand() % diff + nsecMin;
    }
    sec = attesa / ntos;
    nsec = attesa - (sec * ntos);
/*TEST*/
#if (ENABLE_TEST)
    printf("SEC: %d\n", sec);
    printf("NSEC: %ld\n", nsec);
#endif
    tempoDiAttesa.tv_sec = sec;
    tempoDiAttesa.tv_nsec = nsec;
    nanosleep(&tempoDiAttesa, NULL);
    /*TODO - MASCERARE IL SEGNALE SIGCONT*/
}

int scegliNumeroNodo(int max)
{
#if (ENABLE_TEST)
    printf("Scelgo coda...\n");
#endif
    int iCoda;
    struct timespec timespecRand;
    clock_gettime(CLOCK_REALTIME, &timespecRand);
    srand(timespecRand.tv_nsec);
    iCoda = rand() % max + 1;
#if (ENABLE_TEST)
    printf("Ho scelto coda%d\n", iCoda);
#endif
    return iCoda;
}

int scegliAmicoNodo(int idNodo, int *puntatoreSharedMemoryAmiciNodi)
{
    int numAmici;
    int amicoRandom;
#if (ENABLE_TEST)
    printf("Nodo arrivato e' %d\n", idNodo);
#endif
    numAmici = puntatoreSharedMemoryAmiciNodi[(idNodo - 1) * (getSoFriendsNum() * 2 + 1)];
#if (ENABLE_TEST)
    printf("Questo nodo ha %d amici:\n", numAmici);
    printf("%d e ha scelto a chi inviare %d che ha pid %d\n", puntatoreSharedMemoryTuttiNodi[idNodo].nodoPid, amicoRandom, puntatoreSharedMemoryTuttiNodi[puntatoreSharedMemoryAmiciNodi[(idNodo - 1) * (getSoFriendsNum() * 2 + 1) + amicoRandom]].nodoPid);
#endif
    amicoRandom = scegliNumeroNodo(numAmici);
    return puntatoreSharedMemoryAmiciNodi[(idNodo - 1) * (getSoFriendsNum() * 2 + 1) + amicoRandom];
}