/*LIBRERIE*/

/*portabilita' piu' compatibilita'*/
#define _GNU_SOURCE
/*input/output*/
#include <stdio.h>
/*effettuare exit()*/
#include <stdlib.h>
/*prelevare valore errno*/
#include <errno.h>
/*operazioni stringhe*/
#include <string.h>
/*per gestire i segnali*/
#include <signal.h>
/*per compatiblità*/
#include <sys/types.h>
/*recuperare i flag delle strutture system V*/
#include <sys/ipc.h>
/*memoria condivisa*/
#include <sys/shm.h>
/*operazione sui semafori*/
#include <sys/sem.h>
/*operazione sulle code di messaggi*/
#include <sys/msg.h>
/*gestire la wait*/
#include <sys/wait.h>
/*per getpid() etc*/
#include <unistd.h>
/*libreria per clock_gettime*/
#include <time.h>
/*per recuperare il limite delle variabili*/
#include <limits.h>
/**********/

/*MACRO*/

/*Permette di cambiare la modalita' tra DEBUG e NON-DEBUG*/
#define ENABLE_TEST 0
/*Permette di discriminare gli utenti, 1 - utente ancora attivo*/
#define USER_OK 1
/*Permette di discriminare gli utenti, 0 - utente non e' attivo*/
#define USER_KO 0
/*Motivi di pura portabilita', permette facilmente impostare campo reward della transazione inerente al NODO*/
#define REWARD_SENDER -1
/*La segente macro viene utilizzata per discriminare l'intero parsificato da un intero long*/
#define IS_LONG 1
#define IS_INT 0
/*Le macro che permette di controllare il ciclo di vita del processo nodo*/
#define NODO_CONTINUE 1
#define NODO_STOP 0
/*La macro richiesta dalla traccia della consegna del compito*/
#define SENDER_NODO -1
/*Le macro che permettono di controllare il ciclo di vita del processo master*/
#define MASTER_CONTINUE 1
#define MASTER_STOP 0
/*Le macro che permettono di controllare il ciclo di vita del processo utente*/
#define UTENTE_CONTINUE 1
#define UTENTE_STOP 0
/*macro per gestire il ciclo di vita in base all'allarme*/
#define ALARM_CONTINUE 1
#define ALARM_STOP 0
/*
Utilizzata nel ciclo d'attesa di terminazione degli utenti.
Se un utente termina prematuramente, questo e' il suo valore di ritorno.
*/
#define EXIT_PREMAT 2
/*Le macro che definiscono diversi ragioni della terminazione della simulazione*/
#define ALLARME_SCATTATO 0
#define NO_UTENTI_VIVI 1
#define REGISTRY_FULL 2
#define TERMINATO_DA_UTENTE 3

/*VARIABILI DEFINITE A RUN-TIME*/
/*process_chain.c*/
extern int SO_BLOCK_SIZE;
extern int SO_TP_SIZE;
/**********/

/*STRUTTURE*/

#ifndef tdef
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
#endif

#ifndef mdef
/*struct per invio del messaggio*/
typedef struct mymsg
{
    long mtype;
    transazione transazione;
    int hops;
} message;
#endif

#ifndef sdef
/*implementazione richiesta da parte dell'utente, serve per alterare il valore del semaforo. E' il 4 parametro della funzione semctl.*/
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
#if defined(__linux__)
    struct seminfo *__buf;
#endif
};
#endif

#ifndef udef
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
#endif

#ifndef ndef
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
#endif
/**********/

/*Variabili globali della simulazione*/
extern int SO_USERS_NUM;
extern int SO_NODES_NUM;
extern int SO_REWARD;
extern long SO_MIN_TRANS_GEN_NSEC;
extern long SO_MAX_TRANS_GEN_NSEC;
extern int SO_RETRY;
extern long SO_MIN_TRANS_PROC_NSEC;
extern long SO_MIN_TRANS_MAX_NSEC;
extern int SO_REGISTRY_SIZE;
extern int SO_BUDGET_INIT;
extern int SO_SIM_SEC;
extern int SO_FRIENDS_NUM;
extern int SO_HOPS;
/**********/

/*le firme dei metodi*/
/*la seguente funzione restituisce la capienza del blocco*/
int getSoBlockSize();
/*la seguente funzione restituisce la capienza della transaction pool*/
int getTpSize();
/*la funzione restituisce SO_USERS_NUM*/
int getSoUsersNum();
/*la funzione imposta SO_USERS_NUM*/
void setSoUsersNum(int u);
/*la funzione restituisce SO_NODES_NUM*/
int getSoNodesNum();
/*la funzione imposta SO_NODES_NUM*/
void setSoNodesNum(int n);
/*la funzione restituisce SO_REWARD*/
int getSoReward();
/*la funzione imposta SO_REWARD*/
void setSoReward(int r);
/*la funzione restituisce SO_MIN_TRANS_GEN_NSEC*/
long getSoMinTransGenNsec();
/*la funzione imposta SO_MIN_TRANS_GEN_NSEC*/
void setSoMinTransGenNsec(long ns);
/*la funzione restituisce SO_MIN_TRANS_GEN_NSEC*/
long getSoMaxTransGenNsec();
/*la funzione imposta SO_MIN_TRANS_GEN_NSEC*/
void setSoMaxTransGenNsec(long ns);
/*la funzione imposta SO_RETRY*/
void setSoRetry(int r);
/*la funzione restituisce SO_RETRY*/
int getSoRetry();
/*la funzione restituisce SO_MIN_TRANS_PROC_NSEC*/
long getSoMinTransProcNsec();
/*la funzione imposta SO_MIN_TRANS_PROC_NSEC*/
void setSoMinTransProcNsec(long ns);
/*la funzione restituisce SO_MIN_TRANS_PROC_NSEC*/
long getSoMaxTransProcNsec();
/*la funzione imposta SO_MIN_TRANS_PROC_NSEC*/
void setSoMaxTransProcNsec(long ns);
/*la funzione restituisce SO_REGISTRY_SIZE*/
int getSoRegistrySize();
/*la funzione imposta SO_REGISTRY_SIZE*/
void setSoRegistrySize(int r);
/*la funzione restituisce SO_BUDGET_INIT*/
int getSoBudgetInit();
/*la funzione imposta SO_BUDGET_INIT*/
void setSoBudgetInit(int b);
/*la funzione restituisce SO_SIM_SEC*/
int getSoSimSec();
/*la funzione imposta SO_SIM_SEC*/
void setSoSimSec(int s);
/*la funzione restituisce SO_FRIENDS_NUM*/
int getSoFriendsNum();
/*la funzione imposta SO_FRIENDS_NUM*/
void setSoFriendsNum(int n);
/*la funzione restituisce SO_HOPS*/
int getSoHops();
/*la funzione imposta SO_HOPS*/
void setSoHops(int n);
/**********/
/**********/
/*la seguente funzione parsifica i parametri*/
long parseLongFromParameters(char const *argv[], int pos);
int parseIntFromParameters(char const *argv[], int pos);
/*la seguente funzione implementa la reserve di un semaforo*/
int semReserve(int semId, int semOp, int semNum, short flag);
/*la seguente funzione implementa la release di un semaforo*/
int semRelease(int semId, int semOp, int semNum, short flag);
/*funzione per eseguire la detach dalla SM*/
int eseguiDetachShm(const void *ptr);
/*funzione per deallocare la SM*/
int eseguiDeallocamentoShm(int idShm);
/**********/
