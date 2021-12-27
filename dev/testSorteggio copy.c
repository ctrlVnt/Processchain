#include <time.h>
#define _GNU_SOURCE
#include <stdio.h>  /*input/output*/
#include <stdlib.h> /*effettuare exit()*/
#include <errno.h>  /*prelevare valore errno*/
#include <string.h> /*operazioni stringhe*/
#include <signal.h> /*per gestire i segnali*/

#include <sys/shm.h>   /*memoria condivisa*/
#include <sys/ipc.h>   /*recuperare i flag delle strutture system V*/
#include <sys/types.h> /*per compatiblità*/
#include <sys/sem.h>   /*operazione sui semafori*/
#include <sys/msg.h>   /*operazione sulle code di messaggi*/
#include <sys/wait.h>  /*gestire la wait*/

#include <unistd.h> /*per getpid() etc*/
#include <time.h>   /*libreria per clock_gettime*/

#define TEST_ERROR                                 \
    if (errno)                                     \
    {                                              \
        fprintf(stderr,                            \
                "%s:%d: PID=%5d: Error %d (%s)\n", \
                __FILE__,                          \
                __LINE__,                          \
                getpid(),                          \
                errno,                             \
                strerror(errno));                  \
    }

int scelgoNodo(){
    int nodoScelto;
    struct timespec tempo;
    clock_gettime(CLOCK_REALTIME, &tempo);
    int ciao = (int)tempo.tv_nsec;
    srand(tempo.tv_nsec);
    nodoScelto = (rand() % ((19 - 1) - 0 + 1)) + 0;
    return nodoScelto;
}

int main(){
    int i;
    /*for (i = 0; i < 50; i++){
        int nodoScelto;
        srand(getpid());
        nodoScelto = rand() % 15;
        printf("estraz %d, esce num: %d\n", i, nodoScelto);
    }
    
    for (i = 0; i < 50; i++) {
        int nodoScelto = (rand() % (10 - 0 + 1)) + 0;
        printf("estraz %d, esce num: %d\n", i, nodoScelto);
    }*/
    for (i = 0; i < 50; i++){
        printf("estraz %d, esce num: %d\n", i, scelgoNodo());
        TEST_ERROR;
    }
    printf("estraz %d, esce num: %d\n", i, scelgoNodo());
}