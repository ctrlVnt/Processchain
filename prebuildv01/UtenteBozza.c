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

/*MACRO*/
#define IS_LONG 1
#define IS_INT 0
#define UTENTE_CONTINUE 1
#define UTENTE_STOP 0
#define USER_OK 1
#define USER_KO 0

/*Proposta lista argomenti ricevuto dalla EXECLP*/
/*
I parametri della simulazione(comuni a tutti gli utenti):
0.Nome file eseguibile
1.SO_MIN_TRANS_GEN_NSEC
2.2 SO_MAX_TRANS_GEN_NSEC
3.ID SM dove salvo gli ID delle code di messaggi -- Magari introdurre un campo HEADER a indicare il numero di code presenti
4.Il mio numero d'ordine
5.ID SM dove si trova libro mastro
6.SO_RETRY
7.SO_BUDGET_INIT
8.ID SM dove ci sono tutti gli pid degli utenti - destinatari
9.ID semaforo per accedere alla coda di mex
*/
/****************************************/

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

/*struct per invio messaggio*/
typedef struct mymsg
{
    long mtype;
    transazione transazione;
} message;

/*struttura che tiene traccia dello stato dell'utente*/
typedef struct utente_ds
{
    /*Macro UTENTE_XX*/
    int stato;
    /*Proposta di RICCARDO*/
    int budget;
    /*getpid()*/
    int userPid;
} utente;

/*struttura che tiene traccia dello stato dell'utente*/
typedef struct nodo_ds
{
    /*id della sua coda di messaggi*/
    int mqId;
    /*Proposta di RICCARDO*/
    int budget;
    /*getpid()*/
    int nodoPid;
    int transazioniPendenti;
} nodo;

/***********/

/*VARIABILI GLOBALI*/
/**/
int soRetry;
/**/
int soBudgetInit;
/**/
long soMinTransGenNsec;
/**/
long soMaxTransGenNsec;
/*ID della SM degli'ID delle CODE di messaggi alla quale devo fare l'attach*/
int idSharedMemoryTuttiNodi;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente gli id di tutte le code di messaggi, attenzione primo campo e' HEADER*/
nodo *puntatoreSharedMemoryTuttiNodi;
/*Id del semaforo che regola l'accesso alla MQ associata al nodo*/
int idSemaforoAccessoCodeMessaggi;
/*
Il mio numero d'ordine
*/
int numeroOrdine;
/*ID della SM dove si trova il libro mastro*/
int idSharedMemoryLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria dove si trova effettivamente il libro mastro*/
transazione *puntatoreSharedMemoryLibroMastro;
/*ID della SM contenente tutti i PID dei processi utenti - visti come destinatari*/
int idSharedMemoryTuttiUtenti;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente i PID degli utenti*/
utente *puntatoreSharedMemoryTuttiUtenti;
/*numero totale di utenti*/
int numeroTotaleUtenti;
/*variabile struttura per effettuare le operazioni sul semaforo*/
struct sembuf operazioniSemaforo;
/*variabile determina ciclo di vita dell'utente*/
int user;

/*******************/

/*variabili strutture per gestire i segnali*/

/*sigaction da settare per gestire ALARM - timer della simulazione*/
struct sigaction sigactionSigusr1Nuova;
/*sigaction dove memorizzare lo stato precedente*/
struct sigaction sigactionSigusr1Precedente;
/*maschera per SIGALARM*/
sigset_t maskSetForSigusr1;
/*sigaction generica - uso generale*/
struct sigaction act;
/*old sigaction generica - uso generale*/
struct sigaction actPrecedente;

/*******************************************/

/*variabli ausiliari*/
/*variabile usata come secondo parametro nella funzione strtol, come puntatore dove la conversionbe della stringa ha avuto fine*/
char *endptr;
int shmdtRisposta;
/********************/

/*FUNZIONI*/

void sigusr1Handler(int sigNum);
int scegliUtenteRandom(int max);
int scegliNumeroCoda(int max);

/**********/

int main(int argc, char const *argv[])
{
    printf("\t%s: utente[%d] e ha ricevuto #%d parametri.\n", argv[0], getpid(), argc);

    soMinTransGenNsec = strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[1], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(soMinTransGenNsec == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    else if((soMinTransGenNsec == LONG_MIN || soMinTransGenNsec == LONG_MAX) && errno == ERANGE)
    {
        perror("Errore ERANGE");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("\tsoMinTransProcNsec: %ld\n", soMinTransGenNsec);
    soMaxTransGenNsec = strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[2], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(soMinTransGenNsec == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    else if((soMaxTransGenNsec == LONG_MIN || soMaxTransGenNsec == LONG_MAX) && errno == ERANGE)
    {
        perror("Errore ERANGE");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("\tsoMaxTransProcNsec: %ld\n", soMaxTransGenNsec);

    /*ID DELLA SH DI TUTTE LE CODE DI MESSAGGI*/
    idSharedMemoryTuttiNodi = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[3], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSharedMemoryTuttiNodi == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("\tidSharedMemoryTutteCodeMessaggi: %d\n", idSharedMemoryTuttiNodi);
    
    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryTuttiNodi = (nodo *)shmat(idSharedMemoryTuttiNodi, NULL, 0);
    if(errno == EINVAL)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    /*Recupero il mio numero d'ordine*/
    numeroOrdine = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[4], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(numeroOrdine == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("\tnumeroOrdineUtente: %d\n", numeroOrdine);

    idSharedMemoryLibroMastro = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[5], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSharedMemoryLibroMastro == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("\tidSharedMemoryLibroMastro: %d\n", idSharedMemoryLibroMastro);
    
    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryLibroMastro = (transazione *)shmat(idSharedMemoryLibroMastro, NULL, 0);
    if(errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }

    /*Recupero SO_RETRY*/
    soRetry = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[6], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(soRetry == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("\tsoRetry: %d\n", soRetry);

    /*Recupero SO_BUDGET_INIT*/
    soBudgetInit = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[7], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(soBudgetInit == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("\tsoBudgetInit: %d\n", soBudgetInit);

    idSharedMemoryTuttiUtenti = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[8], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSharedMemoryTuttiUtenti == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("\tidSharedMemoryTuttiUtenti: %d\n", idSharedMemoryTuttiUtenti);
    
    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryTuttiUtenti = (utente *)shmat(idSharedMemoryTuttiUtenti, NULL, 0);
    if(errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }
    numeroTotaleUtenti = puntatoreSharedMemoryTuttiUtenti[0].userPid;
    printf("\tNumero totale di Utenti: %d\n", puntatoreSharedMemoryTuttiUtenti[0].userPid);

    idSemaforoAccessoCodeMessaggi = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[9], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSemaforoAccessoCodeMessaggi == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    printf("\tidSemaforoAccessoCodeMessaggi: %d\n", idSemaforoAccessoCodeMessaggi);

    /*imposto l'handler*/
    sigactionSigusr1Nuova.sa_flags = 0;
    sigemptyset(&sigactionSigusr1Nuova.sa_mask);
    sigactionSigusr1Nuova.sa_handler = sigusr1Handler;
    sigaction(SIGUSR1, &sigactionSigusr1Nuova, &sigactionSigusr1Precedente);

    /*ciclo di vita utente*/
    user = UTENTE_CONTINUE;
    while(user)
    {
        printf("%d ha scelto %d e la coda ordine ID %d\n", getpid(), scegliUtenteRandom(numeroTotaleUtenti), puntatoreSharedMemoryTuttiNodi[scegliNumeroCoda(puntatoreSharedMemoryTuttiNodi[0].nodoPid)].mqId);
        sleep(2);
    }

    /*fine ciclo di vita utente*/

    //*Deallocazione delle risorse*/
    shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiUtenti);
    if(shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryLibroMastro);
    if(shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiNodi);
    if(shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryTutteCodeMessaggi");
        exit(EXIT_FAILURE);
    }
    printf("Risorse deallocate correttamente\n");
    return 0;
}

void sigusr1Handler(int sigNum)
{
    printf("UTENTE SIGUSR1 ricevuto\n");
    user = UTENTE_STOP;
}

/*funzione che restituisce il pid del utente random*/
int scegliUtenteRandom(int max)
{
    int pidRandom;
    int iRandom;
    do
    {
        iRandom = rand()%max + 1;
        if(puntatoreSharedMemoryTuttiUtenti[iRandom].stato != USER_KO)
        {
            pidRandom = puntatoreSharedMemoryTuttiUtenti[iRandom].userPid;
        }
    }
    while(pidRandom == getpid());

    return pidRandom;
}

int scegliNumeroCoda(int max)
{
    srand(rand()%getpid()+1);
    int iCoda;
    iCoda = rand()%max + 1;

    return iCoda;
}