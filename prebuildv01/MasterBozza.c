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
/*
Utilizzata nel ciclo d'attesa di terminazione degli utenti.
Se un utente termina prematuramente, questo e' il suo valore di ritorno.
*/
#define EXIT_PREMAT 2
/*Utilizzata per far andare o meno il processo master*/
#define MASTER_CONTINUE 1
#define MASTER_STOP 0
/*******/

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

/*struttura che tiene traccia dello stato dell'utente*/
typedef struct utente_ds
{
    /*Macro UTENTE_XX*/
    int stato;
    /*getpid()*/
    int userPid;
} utente;

/***********/

/*VARIABILI GLOBALI*/

/*PARAMETRI LETTI A RUN-TIME*/

/*Numero totale di utenti-partecipanti della simulazione*/
int SO_USERS_NUM;
/*Numero dei nodi attivi durante la simulazion che gestiscono le transazioni*/
int SO_NODES_NUM;
/*Un valore che ogni utente deve pagare al nodo scelto per effettuare la transazione*/
int SO_REWARD;
/*Tempo di attesa minimo da parte dell'utente tra un ciclo di vita e l'altro*/
long SO_MIN_TRANS_GEN_NSEC;
/*Tempo di attesa massimo da parte dell'utente tra un ciclo di vita e l'altro*/
long SO_MAX_TRANS_GEN_NSEC;
/*Numero di "vite" assegnato al NODO, diminuisce nel caso quest'ultimo non riesca a inviare alcuna transazione*/
int SO_RETRY;
/*Tempo di attesa minimo da parte del nodo tra un ciclo di vita e l'altro*/
long SO_MIN_TRANS_PROC_NSEC;
/*Tempo di attesa massimo da parte del nodo tra un ciclo di vita e l'altro*/
long SO_MAX_TRANS_PROC_NSEC;
/*La capacita' massima del libro mastro*/
int SO_REGISTRY_SIZE;
/*Budget iniziale attribuito ad ogni utente*/
int SO_BUDGET_INIT;
/*Limite di tempo della simulazione*/
int SO_SIM_SEC;
/*VERSIONE FULL - Indica il numero di processi nodi-amici*/
int SO_FRIENDS_NUM;

/****************************/

/*PARAMETRI LETTI A COMPILE TIME*/

int SO_BLOCK_SIZE;
int SO_TP_SIZE;

/********************************/

/*ID SM*/

/*ID della SM contenente il libro mastro*/
int idSharedMemoryLibroMastro;
/*ID SM dove si trova l'indice del libro mastro*/
int idSharedMemoryIndiceLibroMastro;
/*ID della SM degli'ID delle CODE di messaggi alla quale devo fare l'attach*/
int idSharedMemoryTutteCodeMessaggi;
/*ID della SM contenente tutti i PID dei processi utenti - visti come destinatari*/
int idSharedMemoryTittiPidUtenti;

/*******/


/*Puntatori SM*/

/*Dopo l'attach, punta alla porzione di memoria dove si trova effettivamente il libro mastro*/
transazione *puntatoreSharedMemoryLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria condivisa dove si trova effettivamente l'indice del libro mastro*/
int *puntatoreSharedMemoryIndiceLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente gli id di tutte le code di messaggi, attenzione primo campo e' HEADER*/
int *puntatoreSharedMemoryTutteCodeMessaggi;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente i PID degli utenti*/
utente *puntatoreSharedMemoryTuttiPidUtenti;

/*************/

/*Semafori*/

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
/*
Id del semaforo che regola l'accesso alla MQ associata al nodo
Il valore iniziale e' pari alla capacita' della transaction pool.
*/
int idSemaforoAccessoNodoCodaMessaggi;
/*Id del semaforo per sincronizzare l'avvio dei processi*/
int idSemaforoSincronizzazioneTuttiProcessi;

/**********/

/*variabile struttura per effettuare le operazioni sul semaforo*/

/*streuttura generica per dialogare con semafori IPC V*/
struct sembuf operazioniSemaforo;
/*array contenente i valori iniziali di ciascun semaforo associato ad ogni MQ*/
unsigned short *arrayValoriInizialiSemaforiCodeMessaggi;
/*struttura utilizzata nella semctl*/
union semun semaforoUnion;

/***************************************************************/

/*variabili strutture per gestire i segnali*/

/*sigaction da settare per gestire ALARM - timer della simulazione*/
struct sigaction sigactionAlarmNuova;
/*sigaction dove memorizzare lo stato precedente*/
struct sigaction sigactionAlarmPrecedente;
/*maschera per SIGALARM*/
sigset_t maskSetForAlarm;
/*sigaction generica - uso generale*/
struct sigaction act;
/*old sigaction generica - uso generale*/
struct sigaction actPrecedente;

/*******************************************/

/*variabili ausiliari*/

int readAllParametersRisposta;
int shmctlRisposta;
int shmdtRisposta;
int semopRisposta;
int semctlRisposta;
int msggetRisposta;
int msgctlRisposta;
int execRisposta;
int i;
int master;
int numeroNodi;
int childPid;
int childPidWait;
int childStatus;
int contatoreUtentiVivi;

/*********************/

/*Variabili necessari per poter avviare i nodi, successivamente gli utenti*/

char parametriPerNodo[10][32];
char intToStrBuff[32];
/*QUESTO UTILIZZIAMO PER NOTIFICARE I NOSTRI CARISSIMI PROCESSI NODI*/
int *arrayPidProcessiNodi;

/**************************************************************************/

/*******************/

/*FUNZIONI*/

/*Inizializza le variabili globali da un file di input TXT. */
int readAllParameters();
/*gestione del segnale*/
void alarmHandler(int sigNum);

/**********/

int main(int argc, char const *argv[])
{
    printf("Sono MASTER[%d]\n", getpid());

    /*Parsing dei parametri a RUN-TIME*/

    readAllParametersRisposta = readAllParameters();
    if(readAllParametersRisposta == -1)
    {
        perror("- fetch parameters");
        exit(EXIT_FAILURE);
    }
    printf("+ Parsing parametri avvenuto correttamente\n");
    contatoreUtentiVivi = SO_USERS_NUM;

    /*Inizializzazione LIBRO MASTRO*/

    /*SM*/
    SO_BLOCK_SIZE = 5;
    idSharedMemoryLibroMastro = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);
    if(idSharedMemoryLibroMastro == -1)
    {
        perror("- shmget idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    printf("+ idSharedMemoryLibroMastro creato con successo - %d\n", idSharedMemoryLibroMastro);
    puntatoreSharedMemoryLibroMastro = (transazione *)shmat(idSharedMemoryLibroMastro, NULL, 0);
    
    if(errno == 22)
    {
        perror("- shmat idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    printf("+ puntatore al libroMastro creato con successo\n");

    /*SEMAFORO*/
    idSemaforoAccessoLibroMastro = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
    if(idSemaforoAccessoLibroMastro == -1)
    {
        perror("semget idSemaforoAccessoLibroMastro");
        exit(EXIT_FAILURE);
    }
    printf("+ idSemaforoAccessoLibroMastro creato con successo - %d\n", idSemaforoAccessoLibroMastro);
    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_num = 0;
    operazioniSemaforo.sem_op = 1;
    semopRisposta = semop(idSemaforoAccessoLibroMastro, &operazioniSemaforo, 1);
    if(semopRisposta == -1)
    {
        perror("semop idSemaforoAccessoLibroMastro");
        exit(EXIT_FAILURE);
    }
    printf("+ semaforo idSemaforoAccessoLibroMastro inizialiizato a 1\n");

    /*FINE Inizializzazione LIBRO MASTRO*/

    /*Inizializzazione INDICE LIBRO MASTRO*/
    
    /*SM*/
    idSharedMemoryIndiceLibroMastro = shmget(IPC_PRIVATE, sizeof(int), 0600 | IPC_CREAT);
    if(idSharedMemoryIndiceLibroMastro == -1)
    {
        perror("- shmget idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    printf("+ idSharedMemoryIndiceLibroMastro creato con successo - %d\n", idSharedMemoryIndiceLibroMastro);
    puntatoreSharedMemoryIndiceLibroMastro = (int *)shmat(idSharedMemoryIndiceLibroMastro, NULL, 0);
    if(*puntatoreSharedMemoryIndiceLibroMastro == -1)
    {
        perror("- shmat idSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    puntatoreSharedMemoryIndiceLibroMastro[0] = 0;
    printf("+ indice creato con successo e inizializzato a %d\n", puntatoreSharedMemoryIndiceLibroMastro[0]);

    /*SEMAFORO*/
    idSemaforoAccessoIndiceLibroMastro = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
    if(idSemaforoAccessoIndiceLibroMastro == -1)
    {
        perror("- semget idSemaforoAccessoIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    printf("+ idSemaforoAccessoIndiceLibroMastro creato con successo - %d\n", idSemaforoAccessoIndiceLibroMastro);
    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_num = 0;
    operazioniSemaforo.sem_op = 1;
    semopRisposta = semop(idSemaforoAccessoIndiceLibroMastro, &operazioniSemaforo, 1);
    if(semopRisposta == -1)
    {
        perror("- semop idSemaforoAccessoLibroMastro");
        exit(EXIT_FAILURE);
    }
    printf("+ semaforo idSemaforoAccessoLibroMastro inizialiizato a 1\n");

    /*FINE Inizializzazione INDICE LIBRO MASTRO*/

    /*INIZIO Inizializzazione SM che contiene gli ID delle code di messaggi, ciascuna associata a un determinato NODO*/
    /*array nodi privato*/
    arrayPidProcessiNodi = (int *)calloc(2*SO_NODES_NUM, sizeof(int));
    if(arrayPidProcessiNodi == NULL)
    {
        perror("calloc arrayPidProcessiNodi");
        exit(EXIT_FAILURE);
    }
    /*SM*/
    /*REMINDER x2, prima cella di questa SM indica il NUMERO totale di CODE presenti, quindi rispecchia anche il numero dei nodi presenti*/
    idSharedMemoryTutteCodeMessaggi = shmget(IPC_PRIVATE, 2 * SO_NODES_NUM + 1, 0600 | IPC_CREAT);
    if(idSharedMemoryTutteCodeMessaggi == -1)
    {
        perror("shmget idSharedMemoryTutteCodeMessaggi");
        exit(EXIT_FAILURE);
    }
    printf("+ idSharedMemoryTutteCodeMessaggi creato con successo - %d\n", idSharedMemoryTutteCodeMessaggi);
    puntatoreSharedMemoryTutteCodeMessaggi = (int *)shmat(idSharedMemoryTutteCodeMessaggi, NULL, 0);
    if(*puntatoreSharedMemoryTutteCodeMessaggi == -1)
    {
        perror("- shmat idSharedMemoryTutteCodeMessaggi");
        exit(EXIT_FAILURE);
    }
    puntatoreSharedMemoryTutteCodeMessaggi[0] = SO_NODES_NUM;
    printf("+ puntatoreSharedMemoryTutteCodeMessaggi creato con successo, numero totale di MQ - %d\n", puntatoreSharedMemoryTutteCodeMessaggi[0]);
    numeroNodi = 0;
    for(numeroNodi; numeroNodi < SO_NODES_NUM; numeroNodi++)
    {
        msggetRisposta = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
        if(msggetRisposta == -1)
        {
            perror("- msgget tutteCodeMessaggi");
            exit(EXIT_FAILURE);
        }
        puntatoreSharedMemoryTutteCodeMessaggi[numeroNodi + 1] = msggetRisposta;
        printf("+ mssget registrata all'indice %d con id - %d\n",  (numeroNodi + 1), msggetRisposta);
    }
    printf("+ registrazione tutteCodeMessaggi avvenuta con successo, totale code %d\n", numeroNodi);
    /*SEMAFORI*/
    idSemaforoAccessoNodoCodaMessaggi = semget(IPC_PRIVATE, 2 * SO_NODES_NUM, 0600 | IPC_CREAT);
    if(idSemaforoAccessoNodoCodaMessaggi == -1)
    {
        perror("- semget idSemaforoAccessoNodoCodaMessaggi");
        exit(EXIT_FAILURE);
    }
    printf("+ idSemaforoAccessoNodoCodaMessaggi inizializzato correttamente con id - %d\n", idSemaforoAccessoNodoCodaMessaggi);
    
    arrayValoriInizialiSemaforiCodeMessaggi = (unsigned short *)calloc(SO_NODES_NUM, sizeof(unsigned int));
    if(arrayValoriInizialiSemaforiCodeMessaggi == NULL)
    {
        perror("- calloc arrayValoriInizialiSemaforiCodeMessaggi");
        exit(EXIT_FAILURE);
    }
    printf("+ arrayValoriInizialiSemaforiCodeMessaggi inizializzato correttamente\n");
    i = 0;
    SO_TP_SIZE = 10;
    for(i; i < SO_NODES_NUM; i++)
    {
        arrayValoriInizialiSemaforiCodeMessaggi[i] = SO_TP_SIZE;
    }
    printf("+ arrayValoriInizialiSemaforiCodeMessaggi popolato correttamente\n");
    semaforoUnion.array = arrayValoriInizialiSemaforiCodeMessaggi;
    semctlRisposta = semctl(idSemaforoAccessoNodoCodaMessaggi, 0, SETALL, semaforoUnion);
    if(semctlRisposta == -1)
    {
        perror("- semctl SETALL");
        exit(EXIT_FAILURE);
    }
    printf("+ set semafori idSemaforoAccessoNodoCodaMessaggi inizializzato correttamente\n");

    /*FINE Inizializzazione SM delle MQ*/
    
    /*INIZIO Inizializzazione SM che contiene i PID degli USER*/

    idSharedMemoryTittiPidUtenti = shmget(IPC_PRIVATE, SO_USERS_NUM, 0600 | IPC_CREAT);
    if(idSharedMemoryTittiPidUtenti == -1)
    {
        perror("- shmget idSharedMemoryTittiPidUtenti");
        exit(EXIT_FAILURE);
    }
    printf("+ idSharedMemoryTittiPidUtenti creato con successo - %d\n", idSharedMemoryTittiPidUtenti);
    
    /*FINE Inizializzazione SM che contiene i PID degli USER*/

    /*INIZIO Semaforo di sincronizzazione*/

    idSemaforoSincronizzazioneTuttiProcessi = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
    if(idSemaforoSincronizzazioneTuttiProcessi == -1)
    {
        perror("- semget idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }
    printf("+ idSemaforoSincronizzazioneTuttiProcessi inizializzato correttamente con id - %d\n", idSemaforoSincronizzazioneTuttiProcessi);
    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_op = SO_NODES_NUM + 1;
    operazioniSemaforo.sem_num = 0;
    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
    if(semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }
    printf("+ inizio sincronizzare NODI\n");
    
    /*CREAZIONE NODI*/

    i = 0;
    for(i; i < SO_NODES_NUM; i++)
    {
        switch((childPid = fork()))
        {
            case -1:
                perror("fork");
                exit(EXIT_FAILURE);
            break;
            case 0:
                /*QUA HO EREDITATO TUTTI I PUNTATORI*/
                /*devo notificare il parent e attendere lo zero*/
                
                operazioniSemaforo.sem_flg = 0;
                operazioniSemaforo.sem_num = 0;
                operazioniSemaforo.sem_op = -1;
                semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
                if(semopRisposta == -1)
                {
                    perror("semop nodo");
                    exit(EXIT_FAILURE);
                }
                operazioniSemaforo.sem_flg = 0;
                operazioniSemaforo.sem_num = 0;
                operazioniSemaforo.sem_op = 0;
                semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
                if(semopRisposta == -1)
                {
                    perror("semop nodo");
                    exit(EXIT_FAILURE);
                }

                /***********************************************/

                /*INIZIO Costruire la lista di parametri*/

                strcpy(parametriPerNodo[0], "NodoBozza");
                sprintf(intToStrBuff, "%ld", SO_MIN_TRANS_PROC_NSEC);
                strcpy(parametriPerNodo[1], intToStrBuff);
                sprintf(intToStrBuff, "%ld", SO_MAX_TRANS_PROC_NSEC);
                strcpy(parametriPerNodo[2], intToStrBuff);
                sprintf(intToStrBuff, "%d", idSharedMemoryTutteCodeMessaggi);
                strcpy(parametriPerNodo[3], intToStrBuff);
                sprintf(intToStrBuff, "%d", /*i-esimo nodo*/i);
                strcpy(parametriPerNodo[4], intToStrBuff);
                sprintf(intToStrBuff, "%d", idSharedMemoryLibroMastro);
                strcpy(parametriPerNodo[5], intToStrBuff);
                sprintf(intToStrBuff, "%d", idSharedMemoryIndiceLibroMastro);
                strcpy(parametriPerNodo[6], intToStrBuff);
                sprintf(intToStrBuff, "%d", idSemaforoAccessoLibroMastro);
                strcpy(parametriPerNodo[7], intToStrBuff);
                sprintf(intToStrBuff, "%d", idSemaforoAccessoIndiceLibroMastro);
                strcpy(parametriPerNodo[8], intToStrBuff);
                sprintf(intToStrBuff, "%d", idSemaforoAccessoNodoCodaMessaggi);
                strcpy(parametriPerNodo[9], intToStrBuff);

                /*FINE Lista*/
                
                printf("+ Tentativo eseguire la execlp\n");
                /*PUNTO FORTE TROVATO - non c'e' da gestire l'array NULL terminated*/
                execRisposta = execlp("./NodoBozza", parametriPerNodo[0], parametriPerNodo[1], parametriPerNodo[2], parametriPerNodo[3], parametriPerNodo[4], parametriPerNodo[5], parametriPerNodo[6], parametriPerNodo[7], parametriPerNodo[8], parametriPerNodo[9], NULL);
                if(execRisposta == -1)
                {
                    perror("execlp");
                    exit(EXIT_FAILURE);
                }
            break;
            default:
                /*NON MI RICORDO SE DEVO FARE QUALCOSA QUA*/
                /*eh invece si'*/
                arrayPidProcessiNodi[i] = childPid;
            break;
        }
    }

    /*FINE CREAZIONE NODI*/

    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_op = -1;
    operazioniSemaforo.sem_num = 0;
    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
    if(semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
        exit(EXIT_FAILURE);
    }
    printf("+ noodi sono notificati correttamente\n");

    sleep(30);
    
    /*FINE Semaforo di sincronizzazione*/




    /*PARTE DEGLI UTENTI - TODO*/






    /*INIZIO Impostazione sigaction per ALARM*/

    /*no signals blocked*/
    sigemptyset(&sigactionAlarmNuova.sa_mask);
    sigactionAlarmNuova.sa_flags = 0;
    sigactionAlarmNuova.sa_handler = alarmHandler;
    sigaction(SIGALRM, &sigactionAlarmNuova, &sigactionAlarmPrecedente);
    printf("+ sigaction per ALARM impostato con successo");
    alarm(SO_SIM_SEC);
    printf("+ timer avviato: %d sec.\n", SO_SIM_SEC);
    /*FINE Impostazione sigaction per ALARM*/

    /*CICLO DI VITA DEL PROCESSO MASTER*/

    master = MASTER_CONTINUE;
    while(master)
    {
        /*TODO*/
        sleep(2);
    }

    /***********************************/

    /*Prima di chiudere le risorse... Attendo i figli hehehe*/
    

    /*Chiusura delle risorse*/

    semctlRisposta = semctl(idSemaforoSincronizzazioneTuttiProcessi, 0, IPC_RMID);
    if(semctlRisposta == -1)
    {
        perror("- semctl idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }
    shmctlRisposta = shmctl(idSharedMemoryTittiPidUtenti, IPC_RMID, NULL);
    if(shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryTittiPidUtenti");
        exit(EXIT_FAILURE);
    }
    semctlRisposta = semctl(idSemaforoAccessoNodoCodaMessaggi, 0, IPC_RMID);
    if(semctlRisposta == -1)
    {
        perror("- semctl idSemaforoAccessoNodoCodaMessaggi");
        exit(EXIT_FAILURE);
    }
    i = 0;
    for(i; i < numeroNodi; i++)
    {
        msgctlRisposta = msgctl(puntatoreSharedMemoryTutteCodeMessaggi[i + 1], IPC_RMID, NULL);
        if(msgctlRisposta == -1)
        {
            perror("- msgctl");/*essere piu' dettagliato*/
            exit(EXIT_FAILURE);
        }
        /*printf("+ codaMessaggi con ID %d eliminata con successo\n", puntatoreSharedMemoryTutteCodeMessaggi[i + 1]);*/
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryTutteCodeMessaggi);
    if(shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryTutteCodeMessaggi");
        exit(EXIT_FAILURE);
    }
    shmctlRisposta = shmctl(idSharedMemoryTutteCodeMessaggi, IPC_RMID, NULL);
    if(shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryTutteCodeMessaggi");
        exit(EXIT_FAILURE);
    }
    semctlRisposta = semctl(idSemaforoAccessoIndiceLibroMastro, /*ignorato*/0, IPC_RMID);
    if(semctlRisposta == -1)
    {
        perror("semctl idSemaforoAccessoIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryIndiceLibroMastro);
    if(shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmctlRisposta = shmctl(idSharedMemoryIndiceLibroMastro, IPC_RMID, NULL);
    if(shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    semctlRisposta = semctl(idSemaforoAccessoLibroMastro, /*ignorato*/0, IPC_RMID);
    if(semctlRisposta == -1)
    {
        perror("semctl idSemaforoAccessoLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryLibroMastro);
    if(shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmctlRisposta = shmctl(idSharedMemoryLibroMastro, IPC_RMID, NULL);
    if(shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    printf("+ Risorse deallocate correttamente\n");
    return 0;/* == exit(EXIT_SUCCESS)*/
}

/*DEFINIZIONE DELLE FUNZIONI*/

int readAllParameters()
{
    char buffer[256]; /*={}*/
    char *token;
    char *delimeter = "= ";
    int parsedValue;
    FILE *configFile = fopen("parameters.txt", "r+");
    int closeResponse;

    bzero(buffer, 256);

    if (configFile != NULL)
    {
        /* printf("File aperto!\n");*/
        while (fgets(buffer, 256, configFile) != NULL) /*comparazione diretta tra fgets e 0 non concessa in pedantic*/
        {
            token = strtok(buffer, delimeter);
            if (strcmp(token, "SO_USERS_NUM") == 0)
            {
                SO_USERS_NUM = atoi(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_NODES_NUM") == 0)
            {
                SO_NODES_NUM = atoi(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_REWARD") == 0)
            {
                SO_REWARD = atoi(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_MIN_TRANS_GEN_NSEC") == 0)
            {
                SO_MIN_TRANS_GEN_NSEC = atol(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_MAX_TRANS_GEN_NSEC") == 0)
            {
                SO_MAX_TRANS_GEN_NSEC = atol(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_RETRY") == 0)
            {
                SO_RETRY = atoi(strtok(NULL, delimeter));
            }
            /*else if (strcmp(token, "SO_TP_SIZE") == 0)
            {
                SO_TP_SIZE = atoi(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_BLOCK_SIZE") == 0)
            {
                SO_BLOCK_SIZE = atoi(strtok(NULL, delimeter));
            }*/
            else if (strcmp(token, "SO_MIN_TRANS_PROC_NSEC") == 0)
            {
                SO_MIN_TRANS_PROC_NSEC = atol(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_MAX_TRANS_PROC_NSEC") == 0)
            {
                SO_MAX_TRANS_PROC_NSEC = atol(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_REGISTRY_SIZE") == 0)
            {
                SO_REGISTRY_SIZE = atoi(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_BUDGET_INIT") == 0)
            {
                SO_BUDGET_INIT = atoi(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_SIM_SEC") == 0)
            {
                SO_SIM_SEC = atoi(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_FRIENDS_NUM") == 0)
            {
                SO_FRIENDS_NUM = atoi(strtok(NULL, delimeter));
            }
            else
            {
                printf("Errore durante il parsing dei parametri - token %s\n", token);
                fclose(configFile);
                return -1;
                /*exit(EXIT_FAILURE);*/
            }

            /*while(token != NULL)
            {
                printf("%s", token);
                token = strtok(NULL, "=");
            }*/
        }
        /* printf("\n");*/
        closeResponse = fclose(configFile);
        if (closeResponse == -1)
        {
            perror("fclose");
            return -1;
            /*exit(EXIT_FAILURE);*/
        }

/* PER TESTING */
#if (ENABLE_TEST)
        printf("Stampo i parametri parsati!\n");
        printf("SO_USERS_NUM=%d\n", SO_USERS_NUM);
        printf("SO_NODES_NUM=%d\n", SO_NODES_NUM);
        printf("SO_REWARD=%d\n", SO_REWARD);
        printf("SO_MIN_TRANS_GEN_NSEC=%ld\n", SO_MIN_TRANS_GEN_NSEC);
        printf("SO_MAX_TRANS_GEN_NSEC=%ld\n", SO_MAX_TRANS_GEN_NSEC);
        printf("SO_RETRY=%d\n", SO_RETRY);
        /*printf("SO_TP_SIZE=%d\n", SO_TP_SIZE);
        printf("SO_BLOCK_SIZE=%d\n", SO_BLOCK_SIZE);*/
        printf("SO_MIN_TRANS_PROC_NSEC=%ld\n", SO_MIN_TRANS_PROC_NSEC);
        printf("SO_MAX_TRANS_PROC_NSEC=%ld\n", SO_MAX_TRANS_PROC_NSEC);
        printf("SO_REGISTRY_SIZE=%d\n", SO_REGISTRY_SIZE);
        printf("SO_BUDGET_INIT=%d\n", SO_BUDGET_INIT);
        printf("SO_SIM_SEC=%d\n", SO_SIM_SEC);
        printf("SO_FRIENDS_NUM=%d\n", SO_FRIENDS_NUM);
#endif

        return 0;
    }
    else
    {
        perror("fopen");
        return -1;
        /*exit(EXIT_FAILURE);*/
    }
}

void alarmHandler(int sigNum)
{
    int cont;
    cont  = 0;
    printf("+ ALARM scattato\n");
    
    for(cont; cont < numeroNodi; cont++)
    {
        kill(arrayPidProcessiNodi[cont], SIGUSR1);
    }
    sleep(1);
    master = MASTER_STOP;
}