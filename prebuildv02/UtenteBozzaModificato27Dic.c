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
#define EXIT_PREMAT 2
#define ENABLE_TEST 0

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
/*La capacita' della transaction pool associata a ciascun nodo per immagazzinare le transazioni da elaborare - NOTA A COMPILE TIME*/
int SO_TP_SIZE;
/*Le transazioni sono elaborate in blocchi, la variabile seguente determina la sua capacita', che dev'essere minore rispetto alla capacita' della transacion pool - NOTA A COMPILE TIME*/
int SO_BLOCK_SIZE;
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
/*ID SM dove si trova l'indice del libro mastro*/
int idSharedMemoryIndiceLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria condivisa dove si trova effettivamente l'indice del libro mastro*/
int *puntatoreSharedMemoryIndiceLibroMastro;
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
int msgsndRisposta;
transazione transazioneInvio;
message messaggio;
int indiceLibroMastroRiservato;
struct timespec timespecRand;
/********************/

/*FUNZIONI*/

void sigusr1Handler(int sigNum);
int scegliUtenteRandom(int max);
int scegliNumeroCoda(int max);
int getQuantitaRandom(int max);
int getBudgetUtente(int indiceLibroMastroRiservato);
void attesaNonAttiva(long nsecMin, long nsecMax);
/**********/

int main(int argc, char const *argv[])
{
    // printf("\t%s: utente[%d] e ha ricevuto #%d parametri.\n", argv[0], getpid(), argc);
    /*INIZIALIZZO VARIABILI A COMPILE TIME*/
<<<<<<< HEAD
    SO_TP_SIZE = 50;
    SO_BLOCK_SIZE = 10;
=======
    SO_TP_SIZE = 10;
    SO_BLOCK_SIZE = 5;
>>>>>>> f600d9b7e00412819030c55f1c41b264b7bf1297
    indiceLibroMastroRiservato = 0;

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
    #if(TEST_ENABLED)
        printf("\tsoMinTransProcNsec: %ld\n", soMinTransGenNsec);
    #endif
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
    #if(TEST_ENABLED)
        printf("\tsoMaxTransProcNsec: %ld\n", soMaxTransGenNsec);
    #endif

    /*ID DELLA SH DI TUTTE LE CODE DI MESSAGGI*/
    idSharedMemoryTuttiNodi = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[3], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSharedMemoryTuttiNodi == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    #if(ENABLE_TEST)
        printf("\tidSharedMemoryTutteCodeMessaggi: %d\n", idSharedMemoryTuttiNodi);
    #endif
    
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
    #if(ENABLE_TEST)
        printf("\tnumeroOrdineUtente: %d\n", numeroOrdine);
    #endif

    idSharedMemoryLibroMastro = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[5], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSharedMemoryLibroMastro == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    #if(ENABLE_TEST)
        printf("\tidSharedMemoryLibroMastro: %d\n", idSharedMemoryLibroMastro);
    #endif
    
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
    #if(ENABLE_TEST)
        printf("\tsoRetry: %d\n", soRetry);
    #endif

    /*Recupero SO_BUDGET_INIT*/
    soBudgetInit = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[7], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(soBudgetInit == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    #if(ENABLE_TEST)
        printf("\tsoBudgetInit: %d\n", soBudgetInit);
    #endif

    idSharedMemoryTuttiUtenti = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[8], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSharedMemoryTuttiUtenti == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    #if(ENABLE_TEST)
        printf("\tidSharedMemoryTuttiUtenti: %d\n", idSharedMemoryTuttiUtenti);
    #endif
    
    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryTuttiUtenti = (utente *)shmat(idSharedMemoryTuttiUtenti, NULL, 0);
    if(errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }
    numeroTotaleUtenti = puntatoreSharedMemoryTuttiUtenti[0].userPid;
    #if(ENABLE_TEST)
        printf("\tNumero totale di Utenti: %d\n", puntatoreSharedMemoryTuttiUtenti[0].userPid);
    #endif

    idSemaforoAccessoCodeMessaggi = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/argv[9], /*PUNTATTORE DI FINE*/&endptr, /*BASE*/10);
    if(idSemaforoAccessoCodeMessaggi == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    #if(ENABLE_TEST)
        printf("\tidSemaforoAccessoCodeMessaggi: %d\n", idSemaforoAccessoCodeMessaggi);
    #endif

    /*Mi atacco alla SM che contiene l'indice del libro mastro*/
    idSharedMemoryIndiceLibroMastro = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[10], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (idSharedMemoryIndiceLibroMastro == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    #if(ENABLE_TEST)
        printf("\tidSharedMemoryIndiceLibroMastro: %d\n", idSharedMemoryIndiceLibroMastro);
    #endif
    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryIndiceLibroMastro = (int *)shmat(idSharedMemoryIndiceLibroMastro, NULL, 0);
    if (*puntatoreSharedMemoryIndiceLibroMastro == -1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    else{
        printf("\tPuntatore esiste!");
    }
    #if(ENABLE_TEST)
        printf("\tValore indiceLibroMastro: %d\n", *(puntatoreSharedMemoryIndiceLibroMastro));
    #endif

    /*imposto l'handler*/
    sigactionSigusr1Nuova.sa_flags = 0;
    sigemptyset(&sigactionSigusr1Nuova.sa_mask);
    sigactionSigusr1Nuova.sa_handler = sigusr1Handler;
    sigaction(SIGUSR1, &sigactionSigusr1Nuova, &sigactionSigusr1Precedente);

    /*ciclo di vita utente*/
    user = UTENTE_CONTINUE;
    int q;
    int idCoda;
    int semopRisposta;
    transazioneInvio.sender = getpid();
    // printf("\t[%d] inizia\n", getpid());
    while(user == UTENTE_CONTINUE)
    {
        //printf("_______UTNTE____________\n");
        //printf("%d ha scelto %d e la coda ordine ID %d\n", getpid(), scegliUtenteRandom(numeroTotaleUtenti), puntatoreSharedMemoryTuttiNodi[scegliNumeroCoda(puntatoreSharedMemoryTuttiNodi[0].nodoPid)].mqId);
        /**/
        indiceLibroMastroRiservato =  getBudgetUtente(indiceLibroMastroRiservato);
        transazioneInvio.receiver = scegliUtenteRandom(numeroTotaleUtenti);
        transazioneInvio.reward = 10;
        idCoda = scegliNumeroCoda(puntatoreSharedMemoryTuttiNodi[0].nodoPid);
        // printf("\nID CODA SCELTO: %d\n", puntatoreSharedMemoryTuttiNodi[idCoda].mqId);
        if(puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget >= 2)
        {
            /*tentativo riservare un posto nella coda*/
            operazioniSemaforo.sem_flg = IPC_NOWAIT;
            operazioniSemaforo.sem_num = idCoda - 1; /*traslo verso sinistra*/
            operazioniSemaforo.sem_op = -1;
            semopRisposta = semop(idSemaforoAccessoCodeMessaggi, &operazioniSemaforo, 1);
            if(errno == EAGAIN)
            {
                /*printf("occupata %d\n", semopRisposta);*/
                soRetry--;
                attesaNonAttiva(soMinTransGenNsec, soMaxTransGenNsec);
            }
            else{
                // printf("[%d] ho abbastanza budget %d\n", getpid(), puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget);
                q = getQuantitaRandom(puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget);
                transazioneInvio.reward = (q *5)/100;
                if(transazioneInvio.reward == 0)
                {
                    transazioneInvio.reward = 1;
                }
                transazioneInvio.quantita = q - transazioneInvio.reward;
                
                clock_gettime(CLOCK_BOOTTIME, &transazioneInvio.timestamp);
                messaggio.mtype = getpid();
                messaggio.transazione = transazioneInvio;
                msgsndRisposta = msgsnd(puntatoreSharedMemoryTuttiNodi[idCoda].mqId, &messaggio, sizeof(messaggio.transazione), 0);
                if(errno == EAGAIN && msgsndRisposta == -1)
                {
                    /*printf("Coda scelta occupata...\n");*/
                    soRetry--;
                }
                else if(msgsndRisposta != -1){
                    //printf("[%d] messaggio inviato con risposta %d\n", getpid(), msgsndRisposta);
                    puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget -= transazioneInvio.quantita;
                    attesaNonAttiva(soMinTransGenNsec, soMaxTransGenNsec);
                }
            }
        }
        else
        {
            // printf("\nFACCIO SO RETRY\n");
            soRetry--;
            attesaNonAttiva(soMinTransGenNsec, soMaxTransGenNsec);
        }
        if(soRetry <= 0)
        {
            user = UTENTE_STOP;
        }
    }

    /*fine ciclo di vita utente*/

    //*Deallocazione delle risorse*/
    shmdtRisposta = shmdt(puntatoreSharedMemoryIndiceLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
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
    if(user == UTENTE_STOP)
    {
        exit(EXIT_PREMAT);
    }
    else{
        exit(EXIT_SUCCESS);
    }
}

void sigusr1Handler(int sigNum)
{
    printf("UTENTE SIGUSR1 ricevuto\n");
    user = UTENTE_STOP;
}

/*funzione che restituisce il pid del utente random*/
int scegliUtenteRandom(int max)
{
    // printf("Scelgo utente\n");
    int pidRandom;
    int iRandom;
    clock_gettime(CLOCK_REALTIME, &timespecRand);
    srand(timespecRand.tv_nsec);
    do
    {
        iRandom = rand()%max + 1;
        if(iRandom == max)
        {
            iRandom--;
        }
        if(puntatoreSharedMemoryTuttiUtenti[iRandom].stato != USER_KO)
        {
            pidRandom = puntatoreSharedMemoryTuttiUtenti[iRandom].userPid;
        }
    }
    while(pidRandom == getpid() || pidRandom <= 0);
    // printf("[%d]Ho scelto: %d\n", getpid(), pidRandom);
    return pidRandom;
}

int scegliNumeroCoda(int max)
{
    // printf("Scelgo coda...\n");
    clock_gettime(CLOCK_REALTIME, &timespecRand);
    srand(timespecRand.tv_nsec);
    int iCoda;
    iCoda = rand()%max + 1;
    // printf("Ho scelto coda%d\n", iCoda);
    return iCoda;
}

int getQuantitaRandom(int max)
{
    clock_gettime(CLOCK_REALTIME, &timespecRand);
    srand(timespecRand.tv_nsec);
    return rand() % (max - 2 + 1) + 2;
}

int getBudgetUtente(int indiceLibroMastroRiservato)
{
    int ultimoIndice;
    int c;
    ultimoIndice = *puntatoreSharedMemoryIndiceLibroMastro;

    for(indiceLibroMastroRiservato; indiceLibroMastroRiservato < ultimoIndice; indiceLibroMastroRiservato++)
    {
        for(c = 0; c < SO_BLOCK_SIZE - 1; c++)
        {
            if(puntatoreSharedMemoryLibroMastro[indiceLibroMastroRiservato*SO_BLOCK_SIZE + c].receiver == getpid())
            {
                puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget += puntatoreSharedMemoryLibroMastro[indiceLibroMastroRiservato*SO_BLOCK_SIZE + c].quantita;
            }
        }
    }
    /*TEST*/
    // printf("[%d] aggiornamento bilancio terminato\n", getpid());
    return indiceLibroMastroRiservato;
}

void attesaNonAttiva(long nsecMin, long nsecMax)
{
    srand(getpid());
    long ntos = 1e9L;
    long diff = nsecMax - nsecMin;
    long attesa = rand() % diff + nsecMin;
    int sec = attesa / ntos;
    long nsec = attesa - (sec * ntos);
    /*TEST*/
    // printf("SEC: %d\n", sec);
    // printf("NSEC: %ld\n", nsec);
    struct timespec tempoDiAttesa;
    tempoDiAttesa.tv_sec = sec;
    tempoDiAttesa.tv_nsec = nsec;
    nanosleep(&tempoDiAttesa, NULL);
    /*TODO - MASCERARE IL SEGNALE SIGCONT*/
    // printf("FINITO DI ATTENDERE!!!!");
}
