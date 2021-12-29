#include "process_chain.h"

/**********/
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

/********************************/

/*ID SM*/

/*ID della SM contenente il libro mastro*/
int idSharedMemoryLibroMastro;
/*ID SM dove si trova l'indice del libro mastro*/
int idSharedMemoryIndiceLibroMastro;
/*ID della SM degli'ID delle CODE di messaggi alla quale devo fare l'attach*/
int idSharedMemoryTuttiNodi;
/*ID della SM contenente tutti i PID dei processi utenti - visti come destinatari*/
int idSharedMemoryTuttiUtenti;

/*******/

/*Puntatori SM*/

/*Dopo l'attach, punta alla porzione di memoria dove si trova effettivamente il libro mastro*/
transazione *puntatoreSharedMemoryLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria condivisa dove si trova effettivamente l'indice del libro mastro*/
int *puntatoreSharedMemoryIndiceLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente gli id di tutte le code di messaggi, attenzione primo campo e' HEADER*/
nodo *puntatoreSharedMemoryTuttiNodi;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente i PID degli utenti*/
utente *puntatoreSharedMemoryTuttiUtenti;

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
int motivoTerminazione;

/*********************/

/*Variabili necessari per poter avviare i nodi, successivamente gli utenti*/

char parametriPerNodo[13][32];
char intToStrBuff[32];
char parametriPerUtente[14][32];
/*QUESTO UTILIZZIAMO PER NOTIFICARE I NOSTRI CARISSIMI PROCESSI NODI*/
int *arrayPidProcessiNodi;

/**************************************************************************/

/*******************/

/*FUNZIONI*/

/*Inizializza le variabili globali da un file di input TXT. */
int readAllParameters();
/*gestione del segnale*/
void alarmHandler(int sigNum);
/*stampa terminale*/
void stampaTerminale();

/**********/

/*VARIABILI PER LA STAMPA*/
utente utenteMax;
utente utenteMin;
nodo nodoMax;
nodo nodoMin;
/********/

int main(int argc, char const *argv[])
{
    printf("Sono MASTER[%d]\n", getpid());

    /*Parsing dei parametri a RUN-TIME*/

    readAllParametersRisposta = readAllParameters();
    if (readAllParametersRisposta == -1)
    {
        perror("- fetch parameters");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ Parsing parametri avvenuto correttamente\n");
#endif
    contatoreUtentiVivi = SO_USERS_NUM;

    /*Inizializzazione LIBRO MASTRO*/

    /*SM*/
    idSharedMemoryLibroMastro = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE *SO_BLOCK_SIZE*sizeof(transazione), 0600 | IPC_CREAT);
    if (idSharedMemoryLibroMastro == -1)
    {
        perror("- shmget idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSharedMemoryLibroMastro creato con successo - %d\n", idSharedMemoryLibroMastro);
#endif
    puntatoreSharedMemoryLibroMastro = (transazione *)shmat(idSharedMemoryLibroMastro, NULL, 0);

    if (errno == 22)
    {
        perror("- shmat idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ puntatore al libroMastro creato con successo\n");
#endif

    /*SEMAFORO*/
    idSemaforoAccessoLibroMastro = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
    if (idSemaforoAccessoLibroMastro == -1)
    {
        perror("semget idSemaforoAccessoLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSemaforoAccessoLibroMastro creato con successo - %d\n", idSemaforoAccessoLibroMastro);
#endif
    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_num = 0;
    operazioniSemaforo.sem_op = 1;
    semopRisposta = semop(idSemaforoAccessoLibroMastro, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("semop idSemaforoAccessoLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ semaforo idSemaforoAccessoLibroMastro inizialiizato a 1\n");
#endif

    /*FINE Inizializzazione LIBRO MASTRO*/

    /*Inizializzazione INDICE LIBRO MASTRO*/

    /*SM*/
    idSharedMemoryIndiceLibroMastro = shmget(IPC_PRIVATE, sizeof(int), 0600 | IPC_CREAT);
    if (idSharedMemoryIndiceLibroMastro == -1)
    {
        perror("- shmget idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TETS)
    printf("+ idSharedMemoryIndiceLibroMastro creato con successo - %d\n", idSharedMemoryIndiceLibroMastro);
#endif
    puntatoreSharedMemoryIndiceLibroMastro = (int *)shmat(idSharedMemoryIndiceLibroMastro, NULL, 0);
    if (*puntatoreSharedMemoryIndiceLibroMastro == -1)
    {
        perror("- shmat idSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    puntatoreSharedMemoryIndiceLibroMastro[0] = 0;
#if (ENABLE_TEST)
    printf("+ indice creato con successo e inizializzato a %d\n", puntatoreSharedMemoryIndiceLibroMastro[0]);
#endif

    /*SEMAFORO*/
    idSemaforoAccessoIndiceLibroMastro = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
    if (idSemaforoAccessoIndiceLibroMastro == -1)
    {
        perror("- semget idSemaforoAccessoIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSemaforoAccessoIndiceLibroMastro creato con successo - %d\n", idSemaforoAccessoIndiceLibroMastro);
#endif
    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_num = 0;
    operazioniSemaforo.sem_op = 1;
    semopRisposta = semop(idSemaforoAccessoIndiceLibroMastro, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoAccessoLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ semaforo idSemaforoAccessoLibroMastro inizialiizato a 1\n");
#endif

    /*FINE Inizializzazione INDICE LIBRO MASTRO*/

    /*INIZIO Inizializzazione SM che contiene gli ID delle code di messaggi, ciascuna associata a un determinato NODO*/
    /*array nodi privato*/
    arrayPidProcessiNodi = (int *)calloc(2 * SO_NODES_NUM, sizeof(int));
    if (arrayPidProcessiNodi == NULL)
    {
        perror("calloc arrayPidProcessiNodi");
        exit(EXIT_FAILURE);
    }
    /*SM*/
    /*REMINDER x2, prima cella di questa SM indica il NUMERO totale di CODE presenti, quindi rispecchia anche il numero dei nodi presenti*/
    idSharedMemoryTuttiNodi = shmget(IPC_PRIVATE, sizeof(nodo) * (2 * SO_NODES_NUM + 1), 0600 | IPC_CREAT);
    if (idSharedMemoryTuttiNodi == -1)
    {
        perror("shmget idSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSharedMemoryTuttiNodi creato con successo - %d\n", idSharedMemoryTuttiNodi);
#endif
    puntatoreSharedMemoryTuttiNodi = (nodo *)shmat(idSharedMemoryTuttiNodi, NULL, 0);
    if (errno == EINVAL)
    {
        perror("- shmat idSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }
    /*continee il numero totale di nodi*/
    puntatoreSharedMemoryTuttiNodi[0].nodoPid = SO_NODES_NUM;
    puntatoreSharedMemoryTuttiNodi[0].budget = -1;
    puntatoreSharedMemoryTuttiNodi[0].mqId = -1;
    puntatoreSharedMemoryTuttiNodi[0].transazioniPendenti = -1;
#if (ENABLE_TEST)
    printf("+ puntatoreSharedMemoryTuttiNodi creato con successo, numero totale di MQ - %d\n", puntatoreSharedMemoryTuttiNodi[0].nodoPid);
#endif
    numeroNodi = 0;
    for (numeroNodi; numeroNodi < SO_NODES_NUM; numeroNodi++)
    {
        msggetRisposta = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
        if (msggetRisposta == -1)
        {
            perror("- msgget tutteCodeMessaggi");
            exit(EXIT_FAILURE);
        }
        puntatoreSharedMemoryTuttiNodi[numeroNodi + 1].mqId = msggetRisposta;
        puntatoreSharedMemoryTuttiNodi[numeroNodi + 1].transazioniPendenti = SO_TP_SIZE;
#if (ENABLE_TEST)
        printf("+ mssget registrata all'indice %d con id - %d\n", (numeroNodi + 1), msggetRisposta);
#endif
    }
#if (ENABLE_TEST)
    printf("+ registrazione tutteCodeMessaggi avvenuta con successo, totale code %d\n", numeroNodi);
#endif
    /*SEMAFORI*/
    idSemaforoAccessoNodoCodaMessaggi = semget(IPC_PRIVATE, 2 * SO_NODES_NUM, 0600 | IPC_CREAT);
    if (idSemaforoAccessoNodoCodaMessaggi == -1)
    {
        perror("- semget idSemaforoAccessoNodoCodaMessaggi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSemaforoAccessoNodoCodaMessaggi inizializzato correttamente con id - %d\n", idSemaforoAccessoNodoCodaMessaggi);
#endif

    arrayValoriInizialiSemaforiCodeMessaggi = (unsigned short *)calloc(SO_NODES_NUM, sizeof(unsigned int));
    if (arrayValoriInizialiSemaforiCodeMessaggi == NULL)
    {
        perror("- calloc arrayValoriInizialiSemaforiCodeMessaggi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ arrayValoriInizialiSemaforiCodeMessaggi inizializzato correttamente\n");
#endif
    i = 0;
    for (i; i < SO_NODES_NUM; i++)
    {
        arrayValoriInizialiSemaforiCodeMessaggi[i] = SO_TP_SIZE;
    }
#if (ENABLE_TEST)
    printf("+ arrayValoriInizialiSemaforiCodeMessaggi popolato correttamente\n");
#endif
    semaforoUnion.array = arrayValoriInizialiSemaforiCodeMessaggi;
    semctlRisposta = semctl(idSemaforoAccessoNodoCodaMessaggi, 0, SETALL, semaforoUnion);
    if (semctlRisposta == -1)
    {
        perror("- semctl SETALL");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ set semafori idSemaforoAccessoNodoCodaMessaggi inizializzato correttamente\n");
#endif

    /*FINE Inizializzazione SM delle MQ*/

    /*INIZIO Inizializzazione SM che contiene i PID degli USER*/

    idSharedMemoryTuttiUtenti = shmget(IPC_PRIVATE, (SO_USERS_NUM + 1) * sizeof(utente), 0600 | IPC_CREAT);
    if (idSharedMemoryTuttiUtenti == -1)
    {
        perror("- shmget idSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSharedMemoryTuttiUtenti creato con successo - %d\n", idSharedMemoryTuttiUtenti);
#endif

    /*FINE Inizializzazione SM che contiene i PID degli USER*/

    /*INIZIO Semaforo di sincronizzazione*/

    idSemaforoSincronizzazioneTuttiProcessi = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
    if (idSemaforoSincronizzazioneTuttiProcessi == -1)
    {
        perror("- semget idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSemaforoSincronizzazioneTuttiProcessi inizializzato correttamente con id - %d\n", idSemaforoSincronizzazioneTuttiProcessi);
#endif
    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_op = SO_NODES_NUM + 1;
    operazioniSemaforo.sem_num = 0;
    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ inizio sincronizzare NODI\n");
#endif

    /*CREAZIONE NODI*/

    i = 0;
    for (i; i < SO_NODES_NUM; i++)
    {
        switch ((childPid = fork()))
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
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }
            operazioniSemaforo.sem_flg = 0;
            operazioniSemaforo.sem_num = 0;
            operazioniSemaforo.sem_op = 0;
            semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }

            /***********************************************/

            /*INIZIO Costruire la lista di parametri*/

            strcpy(parametriPerNodo[0], "NodoBozza.out");
            sprintf(intToStrBuff, "%ld", SO_MIN_TRANS_PROC_NSEC);
            strcpy(parametriPerNodo[1], intToStrBuff);
            sprintf(intToStrBuff, "%ld", SO_MAX_TRANS_PROC_NSEC);
            strcpy(parametriPerNodo[2], intToStrBuff);
            sprintf(intToStrBuff, "%d", idSharedMemoryTuttiNodi);
            strcpy(parametriPerNodo[3], intToStrBuff);
            sprintf(intToStrBuff, "%d", /*i-esimo nodo*/ i);
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
            sprintf(intToStrBuff, "%d", SO_TP_SIZE);
            strcpy(parametriPerNodo[10], intToStrBuff);
            sprintf(intToStrBuff, "%d", SO_BLOCK_SIZE);
            strcpy(parametriPerNodo[11], intToStrBuff);
            sprintf(intToStrBuff, "%d", SO_REGISTRY_SIZE);
            strcpy(parametriPerNodo[12], intToStrBuff);

            /*FINE Lista*/

            // printf("+ Tentativo eseguire la execlp\n");
            /*PUNTO FORTE TROVATO - non c'e' da gestire l'array NULL terminated*/
            execRisposta = execlp("./NodoBozza.out", parametriPerNodo[0], parametriPerNodo[1], parametriPerNodo[2], parametriPerNodo[3], parametriPerNodo[4], parametriPerNodo[5], parametriPerNodo[6], parametriPerNodo[7], parametriPerNodo[8], parametriPerNodo[9], parametriPerNodo[10], parametriPerNodo[11], parametriPerNodo[12], NULL);
            if (execRisposta == -1)
            {
                perror("execlp");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            /*NON MI RICORDO SE DEVO FARE QUALCOSA QUA*/
            /*eh invece si'*/
            puntatoreSharedMemoryTuttiNodi[i + 1].budget = 0;
            puntatoreSharedMemoryTuttiNodi[i + 1].nodoPid = childPid;
            break;
        }
    }

    /*FINE CREAZIONE NODI*/

    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_op = -1;
    operazioniSemaforo.sem_num = 0;
    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ nodi sono notificati correttamente\n");
#endif

    /*FINE Semaforo di sincronizzazione*/

    /*PARTE DEGLI UTENTI*/
    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_op = SO_USERS_NUM + 1;
    operazioniSemaforo.sem_num = 0;
    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ inizio sincronizzare UTENTI\n");
#endif

    i = 0;
    /*creo la SM dove memorizzo gli utenti*/
    puntatoreSharedMemoryTuttiUtenti = (utente *)shmat(idSharedMemoryTuttiUtenti, NULL, 0);
    if (errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }

    puntatoreSharedMemoryTuttiUtenti[0].budget = -1;
    puntatoreSharedMemoryTuttiUtenti[0].stato = -1;
    puntatoreSharedMemoryTuttiUtenti[0].userPid = SO_USERS_NUM;

    for (i; i < SO_USERS_NUM; i++)
    {
        switch (childPid = fork())
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
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }
            operazioniSemaforo.sem_flg = 0;
            operazioniSemaforo.sem_num = 0;
            operazioniSemaforo.sem_op = 0;
            semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }

            /***********************************************/

            /*INIZIO Costruire la lista di parametri*/
            strcpy(parametriPerUtente[0], "UtenteBozza.out");
            sprintf(intToStrBuff, "%ld", SO_MIN_TRANS_GEN_NSEC);
            strcpy(parametriPerUtente[1], intToStrBuff);
            sprintf(intToStrBuff, "%ld", SO_MAX_TRANS_GEN_NSEC);
            strcpy(parametriPerUtente[2], intToStrBuff);
            sprintf(intToStrBuff, "%d", idSharedMemoryTuttiNodi);
            strcpy(parametriPerUtente[3], intToStrBuff);
            sprintf(intToStrBuff, "%d", /*i-esimo nodo*/ i);
            strcpy(parametriPerUtente[4], intToStrBuff);
            sprintf(intToStrBuff, "%d", idSharedMemoryLibroMastro);
            strcpy(parametriPerUtente[5], intToStrBuff);
            sprintf(intToStrBuff, "%d", SO_RETRY);
            strcpy(parametriPerUtente[6], intToStrBuff);
            sprintf(intToStrBuff, "%d", SO_BUDGET_INIT);
            strcpy(parametriPerUtente[7], intToStrBuff);
            sprintf(intToStrBuff, "%d", idSharedMemoryTuttiUtenti);
            strcpy(parametriPerUtente[8], intToStrBuff);
            sprintf(intToStrBuff, "%d", idSemaforoAccessoNodoCodaMessaggi);
            strcpy(parametriPerUtente[9], intToStrBuff);
            sprintf(intToStrBuff, "%d", idSharedMemoryIndiceLibroMastro);
            strcpy(parametriPerUtente[10], intToStrBuff);
            sprintf(intToStrBuff, "%d", SO_REWARD);
            strcpy(parametriPerUtente[11], intToStrBuff);
            sprintf(intToStrBuff, "%d", SO_TP_SIZE);
            strcpy(parametriPerUtente[12], intToStrBuff);
            sprintf(intToStrBuff, "%d", SO_BLOCK_SIZE);
            strcpy(parametriPerUtente[13], intToStrBuff);

            // printf("+ Tentativo eseguire la execlp\n");
            /*PUNTO FORTE TROVATO - non c'e' da gestire l'array NULL terminated*/
            execRisposta = execlp("./UtenteBozza.out", parametriPerUtente[0], parametriPerUtente[1], parametriPerUtente[2], parametriPerUtente[3], parametriPerUtente[4], parametriPerUtente[5], parametriPerUtente[6], parametriPerUtente[7], parametriPerUtente[8], parametriPerUtente[9], parametriPerUtente[10], parametriPerUtente[11], parametriPerUtente[12], parametriPerUtente[13], NULL);
            if (execRisposta == -1)
            {
                perror("execlp");
                exit(EXIT_FAILURE);
            }

            break;
        default:
            puntatoreSharedMemoryTuttiUtenti[i + 1].userPid = childPid;
            puntatoreSharedMemoryTuttiUtenti[i + 1].stato = USER_OK;
            puntatoreSharedMemoryTuttiUtenti[i + 1].budget = SO_BUDGET_INIT;
            /**/
#if (ENABLE_TEST)
            // printf("+ %d UTENTE[%d] registrato correttamente\n", i, childPid);
#endif
            // sleep(1);
            break;
        }
    }

    /*FINE CREAZIONE UTENTI*/

    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_op = -1;
    operazioniSemaforo.sem_num = 0;
    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ utenti sono notificati correttamente\n");
#endif

    /*INIZIO Impostazione sigaction per ALARM*/
    /*no signals blocked*/
    sigemptyset(&sigactionAlarmNuova.sa_mask);
    sigactionAlarmNuova.sa_flags = 0;
    sigactionAlarmNuova.sa_handler = alarmHandler;
    sigaction(SIGALRM, &sigactionAlarmNuova, &sigactionAlarmPrecedente);
#if (ENABLE_TEST)
    printf("+ sigaction per ALARM impostato con successo");
#endif
    alarm(SO_SIM_SEC);
#if (ENABLE_TEST)
    printf("+ timer avviato: %d sec.\n", SO_SIM_SEC);
#endif
    /*FINE Impostazione sigaction per ALARM*/

    /*CICLO DI VITA DEL PROCESSO MASTER*/

    master = MASTER_CONTINUE;
    while (master)
    {
        /*TODO*/
        sleep(1);
        if (puntatoreSharedMemoryIndiceLibroMastro[0] == SO_REGISTRY_SIZE)
        {
            raise(SIGALRM);
            motivoTerminazione = REGISTRY_FULL;
            stampaTerminale(1);
            master = MASTER_STOP;
            break; /*perhe' non voglio che venga eseguito codice sottostante, alterniativa spostare questo pezzo alla fine*/
        }
        /*stampo INFO */
        stampaTerminale(0);
        /**/
        /*verifico se qualcuno ha cambiato lo stato senza attendere*/
        int a = 0;
        if ((childPidWait = waitpid(-1, &childStatus, WNOHANG)) != -1)
        {
            if (WIFEXITED(childStatus))
            {
                /*se lo status e' EXIT_PREMAT*/
                /*if (WEXITSTATUS(childStatus) == EXIT_PREMAT)*/
               /* {*/
                /*faccio notrare a tutti che il numero degli utenti si e' diminuito*/
                for(a = 1; a <= SO_USERS_NUM; a++)
                {
                    if(puntatoreSharedMemoryTuttiUtenti[a].userPid == childPidWait)
                    {
                        puntatoreSharedMemoryTuttiUtenti[a].stato = USER_KO;
                        puntatoreSharedMemoryTuttiUtenti[0].userPid--;/*^*/
                    }
                    /*prima era qui*/
                }
                /*nel caso non ci siano piu' figli, oppure e' rimasto un figlio solo -- termino la simulazione*/
                if (puntatoreSharedMemoryTuttiUtenti[0].userPid <= 1)
                {
                    /*COME SE ALLARME SCATASSE*/
                    // stampaTerminale(1);
                    raise(SIGALRM);
                    master = MASTER_STOP;
                    motivoTerminazione = NO_UTENTI_VIVI;
                }
               /* }*/
            }
        }
    }

    /***********************************/
    /*Prima di chiudere le risorse... Attendo i figli hehehe*/
    while ((childPidWait = waitpid(-1, &childStatus, 0)) != -1)
    {
        // printf("+ %d ha terminato con status %d\n", childPidWait, WEXITSTATUS(childStatus));
    }
    /********************************************************/
    /*STAMPO CON MOTIVO DELLA TERMINAZIONE - flag = 1*/
    stampaTerminale(1);
    /**********************************/
    /*void stampaLibroMastro()
/*{*/
    printf("RECEIVER(PID)|SENDER(PID)|QUANTITA|SEC_ELAPSED\n");
    for(int i = 0; i < *puntatoreSharedMemoryIndiceLibroMastro; i++)
    {
        for(int j = 0; j < SO_BLOCK_SIZE; j++)
        {
            printf("|%010d||%010d||%010d||%s\n", puntatoreSharedMemoryLibroMastro[SO_BLOCK_SIZE*i+j].receiver, puntatoreSharedMemoryLibroMastro[SO_BLOCK_SIZE*i+j].sender, puntatoreSharedMemoryLibroMastro[SO_BLOCK_SIZE*i+j].quantita, ctime(&puntatoreSharedMemoryLibroMastro[SO_BLOCK_SIZE*i+j].timestamp.tv_sec));
        }
        printf("\n");
    }
    printf("Indice: %d\n", puntatoreSharedMemoryIndiceLibroMastro[0]);
/*}*/
    /*Chiusura delle risorse*/
    shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiUtenti);
    if (shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }
    semctlRisposta = semctl(idSemaforoSincronizzazioneTuttiProcessi, 0, IPC_RMID);
    if (semctlRisposta == -1)
    {
        perror("- semctl idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }
    shmctlRisposta = shmctl(idSharedMemoryTuttiUtenti, IPC_RMID, NULL);
    if (shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }
    semctlRisposta = semctl(idSemaforoAccessoNodoCodaMessaggi, 0, IPC_RMID);
    if (semctlRisposta == -1)
    {
        perror("- semctl idSemaforoAccessoNodoCodaMessaggi");
        exit(EXIT_FAILURE);
    }
    i = 0;
    for (i; i < numeroNodi; i++)
    {
        msgctlRisposta = msgctl(puntatoreSharedMemoryTuttiNodi[i + 1].mqId, IPC_RMID, NULL);
        if (msgctlRisposta == -1)
        {
            perror("- msgctl"); /*essere piu' dettagliato*/
            exit(EXIT_FAILURE);
        }
        /*printf("+ codaMessaggi con ID %d eliminata con successo\n", puntatoreSharedMemoryTuttiNodi[i + 1]);*/
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiNodi);
    if (shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }
    shmctlRisposta = shmctl(idSharedMemoryTuttiNodi, IPC_RMID, NULL);
    if (shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }
    semctlRisposta = semctl(idSemaforoAccessoIndiceLibroMastro, /*ignorato*/ 0, IPC_RMID);
    if (semctlRisposta == -1)
    {
        perror("semctl idSemaforoAccessoIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryIndiceLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmctlRisposta = shmctl(idSharedMemoryIndiceLibroMastro, IPC_RMID, NULL);
    if (shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    semctlRisposta = semctl(idSemaforoAccessoLibroMastro, /*ignorato*/ 0, IPC_RMID);
    if (semctlRisposta == -1)
    {
        perror("semctl idSemaforoAccessoLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmctlRisposta = shmctl(idSharedMemoryLibroMastro, IPC_RMID, NULL);
    if (shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    printf("+ Risorse deallocate correttamente\n");
    return 0; /* == exit(EXIT_SUCCESS)*/
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
    cont = 0;
    printf("+ ALARM scattato\n");

    /*header della SM che contine tutti i pid rispecchia il numero dei nodi*/
    if (puntatoreSharedMemoryTuttiUtenti[0].userPid >= 1)
    {
        for (cont = 0; cont < SO_USERS_NUM; cont++)
        {
            if (puntatoreSharedMemoryTuttiUtenti[cont + 1].stato == USER_OK)
            {
                kill(puntatoreSharedMemoryTuttiUtenti[cont + 1].userPid, SIGUSR1);
            }
        }
    }

    cont = 0;
    for (cont; cont < puntatoreSharedMemoryTuttiNodi[0].nodoPid; cont++)
    {
        kill(puntatoreSharedMemoryTuttiNodi[cont + 1].nodoPid, SIGUSR1);
    }
    sleep(1);
    master = MASTER_STOP;
    motivoTerminazione = ALLARME_SCATTATO;
}

void stampaTerminale(int flag)
{

    int contatoreStampa;
    int contPremat;
    char *ragione;

    if (flag == 1)
    {
        ragione = "forza maggiore\0";
        switch (motivoTerminazione)
        {
        case ALLARME_SCATTATO:
            ragione = "l'allarme scattato\0";
            break;
        case NO_UTENTI_VIVI:
            ragione = "no utenti vivi\0";
            break;
        case REGISTRY_FULL:
            ragione = "registro raggiunto capienza massima\0";
            break;
        default:
            break;
        }
        printf("Ragione della terminazione: %s\n", ragione);
    }

    contPremat = 0;
    /*stampo bilancio utenti*/
    contatoreStampa = 0;
    if (SO_USERS_NUM < 20)
    {
        printf("UTENTE[PID] | BILANCIO[INT] | STATO\n");
        for (contatoreStampa; contatoreStampa < SO_USERS_NUM; contatoreStampa++)
        {
            printf("%09d\t%09d\t%09d\n", puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].userPid, puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].budget, puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].stato);
            if (puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].stato == USER_KO)
            {
                contPremat++;
            }
        }
    }else{
        utenteMax.budget = 0;
        utenteMin.budget = SO_BUDGET_INIT;
        for (contatoreStampa; contatoreStampa < SO_USERS_NUM; contatoreStampa++)
        {
           if(puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].budget > utenteMax.budget && puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].stato != USER_KO){
               utenteMax = puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1];
           }
           if(puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].budget < utenteMin.budget && puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].stato != USER_KO){
               utenteMin = puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1];
           }
           if (puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].stato == USER_KO)
            {
                contPremat++;
            }
        }
        printf("UTENTE[PID] | BILANCIO[INT] | STATO\n");
        printf("%09d\t%09d\t%09d <-- UTENTE con budget MAGGIORE\n", utenteMax.userPid, utenteMax.budget, utenteMax.stato);
        printf("%09d\t%09d\t%09d <-- UTENTE con budget MINORE\n", utenteMin.userPid, utenteMin.budget, utenteMin.stato);
    }
    if (flag == 1)
    {
        printf("*******\n# Utenti terminati prematuramente: %d / %d\n**********\n", contPremat, SO_USERS_NUM);
        master = MASTER_STOP;
    }
    contatoreStampa = 0;
    if (SO_NODES_NUM < 20)
    {
        printf("NODO[PID] | BILANCIO[INT] | TRANSAZIONI PENDENTI\n");
        for (contatoreStampa; contatoreStampa < puntatoreSharedMemoryTuttiNodi[0].nodoPid; contatoreStampa++)
        {
            printf("%09d\t%09d\t%09d\n", puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].nodoPid, puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].budget, puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].transazioniPendenti);
        }
    }else{
        for (contatoreStampa; contatoreStampa < puntatoreSharedMemoryTuttiNodi[0].nodoPid; contatoreStampa++)
        {
            nodoMax.budget = 0;
            nodoMin.budget = SO_BUDGET_INIT;
            if(puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].budget > nodoMax.budget && puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].budget != 0){
                nodoMax = puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1];
            }
            if(puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].budget < nodoMin.budget){
                nodoMin = puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1];
            }
        }
        printf("NODO[PID] | BILANCIO[INT] | TRANSAZIONI PENDENTI\n");
        printf("%09d\t%09d\t%09d <-- NODO con budget MAGGIORE\n", nodoMax.nodoPid, nodoMax.budget, nodoMax.transazioniPendenti);
        printf("%09d\t%09d\t%09d <-- NODO con budget MINORE\n", nodoMin.nodoPid, nodoMin.budget, nodoMin.transazioniPendenti);
    }
    printf("*******\nNumero di blocchi: %d\n", *(puntatoreSharedMemoryIndiceLibroMastro));
}