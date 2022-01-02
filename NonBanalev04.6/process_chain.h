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
/*MACRO*/
#define IS_LONG 1
#define IS_INT 0
#define NODO_CONTINUE 1
#define NODO_STOP 0
#define SENDER_NODO -1
/*
Utilizzata nel ciclo d'attesa di terminazione degli utenti.
Se un utente termina prematuramente, questo e' il suo valore di ritorno.
*/
#define EXIT_PREMAT 2
/*Utilizzata per far andare o meno il processo master*/
#define MASTER_CONTINUE 1
#define MASTER_STOP 0
/*ragione della teminazione*/
#define ALLARME_SCATTATO 0
#define NO_UTENTI_VIVI 1
#define REGISTRY_FULL 2
#define TERMINATO_DA_UTENTE 3
/*******/
#define IS_LONG 1
#define IS_INT 0
#define UTENTE_CONTINUE 1
#define UTENTE_STOP 0
#define USER_OK 1
#define USER_KO 0
#define EXIT_PREMAT 2
#define BLOCK 3;
#define TP 10;

extern int SO_BLOCK_SIZE;
extern int SO_TP_SIZE;
