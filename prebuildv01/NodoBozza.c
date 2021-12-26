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
#include <limits.h> /*per recuperare il limite delle variabili*/

/*Proposta lista argomenti ricevuto dalla EXECLP*/
/*
I parametri della simulazione(comuni a tutti i nodi):
0.Nome file eseguibile
1.SO_MIN_TRANS_PROC_NSEC
2.2 SO_MAX_TRANS_PROC_NSEC
3.ID SM dove salvo gli ID delle code di messaggi -- Magari introdurre un campo HEADER a indicare il numero di code presenti
4.1 Il mio numero d'ordine
5.ID SM dove si trova libro mastro
6.ID SM dove si trova l'indice del libro mastro
7.ID semaforo del libro mastro
8.ID semaforo dell'indice libro mastro
9.ID semaforo Mia coda di messaggi
*/
/****************************************/

/*MACRO*/
#define IS_LONG 1
#define IS_INT 0
#define NODO_CONTINUE 1
#define NODO_STOP 0

/*STRUTTURE*/

/*inviata dal processo utente a uno dei processi nodo che la gestisce*/
typedef struct transazione_ds
{
    /*tempo transazione*/
    struct timespec timestamp;
    /*valore sender (getpid())*/
    pid_t sender;
    /*valore receiver -> valore preso da array processi UTENTE*/
    pid_t receiver;
    /*denaro inviato*/
    int quantita;
    /*denaro inviato al nodo*/
    int reward;
} transazione;

/***********/

/*VARIABILI GLOBALI*/
/*La capacita' della transaction pool associata a ciascun nodo per immagazzinare le transazioni da elaborare - NOTA A COMPILE TIME*/
int SO_TP_SIZE;
/*Le transazioni sono elaborate in blocchi, la variabile seguente determina la sua capacita', che dev'essere minore rispetto alla capacita' della transacion pool - NOTA A COMPILE TIME*/
int SO_BLOCK_SIZE;
/**/
long soMinTransProcNsec;
/**/
long soMaxTransProcNsec;
/*ID della SM degli'ID delle CODE di messaggi alla quale devo fare l'attach*/
int idSharedMemoryTutteCodeMessaggi;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente gli id di tutte le code di messaggi, attenzione primo campo e' HEADER*/
int *puntatoreSharedMemoryTutteCodeMessaggi;
/*
Il mio numero d'ordine
Si assume che alla cella numeroOrdineMiaCodaMessaggi-esima nella SM puntato da 
 *puntatoreSharedMemoryTutteCodeMessaggi posso effettivamente recuperare l'ID della mia MQ.
*/
int numeroOrdineMiaCodaMessaggi;
/*Id della mia coda di messaggi*/
int idMiaMiaCodaMessaggi;
/*ID della SM dove si trova il libro mastro*/
int idSharedMemoryLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria dove si trova effettivamente il libro mastro*/
transazione *puntatoreSharedMemoryLibroMastro;
/*ID SM dove si trova l'indice del libro mastro*/
int idSharedMemoryIndiceLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria condivisa dove si trova effettivamente l'indice del libro mastro*/
int *puntatoreSharedMemoryIndiceLibroMastro;
/*
Id del semaforo che regola l'input sul libro mastro
Il semaforo di tipo binario.
*/
int idSemaforoAccessoLibroMastro;
/*
Id del semaforo che regola l'accesso all'indice
Il semaforo di tipo binario.
*/
int idSemaforoAccessoIndiceLibroMastro;
/*Id del semaforo che regola l'accesso alla MQ associata al nodo*/
int idSemaforoAccessoMiaCodaMessaggi;
/*variabili ausiliari*/
/*variabile usata come secondo parametro nella funzione strtol, come puntatore dove la conversionbe della stringa ha avuto fine*/
char *endptr;
/*
quasta variabile contiene il numero di numero totale dei nodi attivi nella simulazione
REMINDER: per recuperare il valore, bisogna accedere alla 0-sime cella della shared memory contenente tutti gli id delle MQ associate 1-1 ad ogni nodo.
*/
int numeroTotaleNodi;
/*variabile struttura per effettuare le operazioni sul semaforo*/
struct sembuf operazioniSemaforo;
/*variabile determina ciclo di vita del nodo*/
int nodo;
/*variabili d'appoggio*/
int shmdtRisposta;

/*variabili strutture per gestire segnali*/

struct sigaction sigactionSigusr1Nuova;
struct sigaction sigactionSigusr1Precedente;
sigset_t maskSetForSigusr1;

/*funzioni relative al nodo*/
void parseThenStoreParameter(int* dest, char const *argv[], int num, int flag);
/*gesrtione del segnale*/
void sigusr1Handler(int sigNum);

int main(int argc, char const *argv[])
{
    printf("%s: nodo[%d] e ha ricevuto #%d parametri.\n", argv[0], getpid(), argc);

    /*INIZIALIZZO VARIABILI A COMPILE TIME*/
    SO_TP_SIZE = 10;
    SO_BLOCK_SIZE = 5;

    /*LIMITI DI TEMPO*/
    /*Recupero primo parametro*/
    soMinTransProcNsec = strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[1], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(soMinTransProcNsec == 0 && errno == EINVAL)
    {
        perror("- Errore di conversione");
        exit(EXIT_FAILURE);
    }
    else if((soMinTransProcNsec == LONG_MIN || soMinTransProcNsec == LONG_MAX) && errno == ERANGE)
    {
        perror("- Errore ERANGE");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("+ soMinTransProcNsec: %ld\n", soMinTransProcNsec);
    
    /*Recupero secondo parametro*/
    soMaxTransProcNsec = strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[2], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(soMaxTransProcNsec == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    else if((soMaxTransProcNsec == LONG_MIN || soMaxTransProcNsec == LONG_MAX) && errno == ERANGE)
    {
        perror("Errore d");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("soMaxTransProcNsec: %ld\n", soMaxTransProcNsec);
    
    /******************/
    
    /*ID DELLA SH DI TUTTE LE CODE DI MESSAGGI*/
    idSharedMemoryTutteCodeMessaggi = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[3], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSharedMemoryTutteCodeMessaggi == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("idSharedMemoryTutteCodeMessaggi: %d\n", idSharedMemoryTutteCodeMessaggi);
    
    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryTutteCodeMessaggi = (int *)shmat(idSharedMemoryTutteCodeMessaggi, NULL, 0);
    if(*puntatoreSharedMemoryTutteCodeMessaggi == -1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    /*Recupero il mio numero d'ordine*/
    numeroOrdineMiaCodaMessaggi = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[4], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(numeroOrdineMiaCodaMessaggi == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("numeroOrdineMiaCodaMessaggi: %d\n", numeroOrdineMiaCodaMessaggi);
    printf("ID mia coda di messaggi - %d\n", puntatoreSharedMemoryTutteCodeMessaggi[numeroOrdineMiaCodaMessaggi + 1]);

    idSharedMemoryLibroMastro = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[5], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSharedMemoryLibroMastro == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("idSharedMemoryLibroMastro: %d\n", idSharedMemoryLibroMastro);

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryLibroMastro = (transazione *)shmat(idSharedMemoryLibroMastro, NULL, 0);
    if(errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }

    idSharedMemoryIndiceLibroMastro = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[6], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSharedMemoryIndiceLibroMastro == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("idSharedMemoryIndiceLibroMastro: %d\n", idSharedMemoryIndiceLibroMastro);

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryIndiceLibroMastro = (int *)shmat(idSharedMemoryIndiceLibroMastro, NULL, 0);
    if(*puntatoreSharedMemoryIndiceLibroMastro == -1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    idSemaforoAccessoLibroMastro = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[7], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSemaforoAccessoLibroMastro == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("idSemaforoAccessoLibroMastro: %d\n", idSemaforoAccessoLibroMastro);

    idSemaforoAccessoIndiceLibroMastro = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[8], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSemaforoAccessoIndiceLibroMastro == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("idSemaforoAccessoIndiceLibroMastro: %d\n", idSemaforoAccessoIndiceLibroMastro);

    idSemaforoAccessoMiaCodaMessaggi = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[9], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSemaforoAccessoMiaCodaMessaggi == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("idSemaforoAccessoMiaCodaMessaggi: %d\n", idSemaforoAccessoMiaCodaMessaggi);

    /*imposto l'handler*/
    sigactionSigusr1Nuova.sa_flags = 0;
    sigemptyset(&sigactionSigusr1Nuova.sa_mask);
    sigactionSigusr1Nuova.sa_handler = sigusr1Handler;
    sigaction(SIGUSR1, &sigactionSigusr1Nuova, &sigactionSigusr1Precedente);

    /*POSSO INIZIARE A GESTIRE LE TRANSAZIONI*/

    nodo = NODO_CONTINUE;
    while(nodo)
    {
        /*TODO*/
        sleep(3);
    }

    /*****************************************/

    /*CICLO DI VITA DEL NODO TERMINA*/
    /*Deallocazione delle risorse*/

    shmdtRisposta = shmdt(puntatoreSharedMemoryIndiceLibroMastro);
    if(shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryLibroMastro);
    if(shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryTutteCodeMessaggi);
    if(shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryTutteCodeMessaggi");
        exit(EXIT_FAILURE);
    }
    printf("Risorse deallocate correttamente\n");
    return EXIT_SUCCESS;
}

/*
La seguente funzione, in base a NUM, recupera il valore dall'array argv e lo salva nella variabile puntata da dest
*/
void parseThenStoreParameter(int* dest, char const *argv[], int num, int flag)
{
    if(flag == IS_LONG || flag == IS_INT)
    {
        *dest = strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[num], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
        if(*dest == 0 && errno == EINVAL)
        {
            perror("Errore di conversione");
            exit(EXIT_FAILURE);
        }
        else if(flag == IS_LONG && (*dest == LONG_MIN || *dest == LONG_MAX) && errno == ERANGE)
        {
            perror("Errore ERANGE");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        fprintf(stderr, "Flag %d non e' definito in questa funzione!\n", flag);
        exit(EXIT_FAILURE);
    }

    return;
}

void sigusr1Handler(int sigNum)
{
    printf("SIGUSR1 ricevuto\n");
    nodo = NODO_STOP;
}