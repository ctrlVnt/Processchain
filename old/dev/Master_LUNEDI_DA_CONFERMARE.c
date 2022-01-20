#define _GNU_SOURCE
#define _POSIX_C_SOURCE
#include <stdio.h>  //input/output
#include <stdlib.h> //effettuare exit()
#include <errno.h>  //prelevare valore errno
#include <string.h> //operazioni stringhe
#include <signal.h>

#include <sys/types.h> //per compatiblità
#include <sys/shm.h>   //memoria condivisa
#include <sys/ipc.h>   //recuperare i flag delle strutture system V
#include <sys/sem.h>   /*operazione sui semafori*/
#include <sys/msg.h>   /*operazione sulle code di messaggi*/
#include <sys/wait.h>

#include <unistd.h> //per getpid() etc
//indicare che è stata copiata dal codice visto a lezione...
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

/*MACRO PER POTER ABILITARE TEST MODE*/
#define ENABLE_TEST 1

typedef struct transazione
{                              //inviata dal processo utente a uno dei processi nodo che la gestisce
    struct timespec timestamp; //tempo transazione
    pid_t sender;              //valore sender (getpid())
    pid_t receiver;            //valore receiver -> valore preso da array processi UTENTE
    int quantita;              //denaro inviato
    int reward;                //denaro inviato al nodo
} transazione;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
#if defined(__linux__)
    struct seminfo *__buf;
#endif
};

/*PER READ ALL PARAMETERS*/
int SO_USERS_NUM;
int SO_NODES_NUM;
int SO_REWARD;
long SO_MIN_TRANS_GEN_NSEC;
long SO_MAX_TRANS_GEN_NSEC;
int SO_RETRY;
int SO_TP_SIZE;
int SO_BLOCK_SIZE;
long SO_MIN_TRANS_PROC_NSEC;
long SO_MAX_TRANS_PROC_NSEC;
int SO_REGISTRY_SIZE;
int SO_BUDGET_INIT;
int SO_SIM_SEC;
int SO_FRIENDS_NUM;

/*Inizializza le variabili globali da un file di input TXT. */
int read_all_parameters();
void alarmHandler(int);
int getRandomUser(int, int, int*);

int main()
{
    /* SOLO PER TEST */
#if (ENABLE_TEST)
    if (read_all_parameters() == -1)
    {
        fprintf(stderr, "Parsing failed!");
    }
#endif

    /****ID DELLE MEMORIE CONDIVISE****/
    int shmIdMastro;
    int shmIdNodo;
    int shmIdUtente;
    int shmIdIndiceBlocco;
    /**********************************/
    /*PUNTATORI ALLE STRUTTURE CONTENUTE ALL'INTERNO DELLE MEMORIE CONDIVISE*/
    int *shmArrayNodeMsgQueuePtr;
    int *shmArrayUsersPidPtr;
    int *shmIndiceBloccoPtr;
    transazione *shmLibroMastroPtr;
    /************************************************************************/
    /*ID DELLE CODE DI MESSAGGI*/
    int msgQueueId;
    /***************************/
    /*ID DEI SEMAFORI*/
    int semSetMsgQueueId;
    int semSyncStartId;
    int semArrUserPidId;
    /***************************/
    /*STRUTTURE DI SUPPORTO PER ESEGUIRE LE OPERAZIONI SUI SEMAFORI*/
    struct sembuf sops;
    union semun semMsgQueueUnion; /*union semctl*/
    /*ATTENZIONE: la riga seguente contiene del codice che non rispetta lo standard c89*/
    /*unsigned short semMsgQueueValArr[SO_NODES_NUM];  /*dichiaro array di union semctl*/
    /*ALTERNATIVA - allocazione dinamica con la calloc*/
    unsigned short *semMsgQueueValArr;
    /***************************************************************/
    /*STRUTTURE PER GESTIONE DEI SEGNALI*/
    struct sigaction sigactionForAlarmNew;
    struct sigaction sigactionForAlarmOld;
    sigset_t maskSetForAlarm;
    /************************************/
    /*VARIABILI AUSILIARI*/
    int i;
    int childPid;
    int childPidWait;
    int childStatus;
    /*********************/

/*TEST SIGACTION*/
#if(ENABLE_TEST)
    sigactionForAlarmNew.sa_flags = 0; /*npo flags*/
    sigactionForAlarmNew.sa_handler = &alarmHandler;
    sigemptyset(&maskSetForAlarm);
    sigactionForAlarmNew.sa_mask = maskSetForAlarm;
    sigaction(SIGALRM, &sigactionForAlarmNew, NULL);

    alarm(1);
#endif

/*SOLO PER TEST*/
#if (ENABLE_TEST)
    union semun TEST_GETALL;
    /*unsigned short TEST_GETALL_ARRAY[SO_NODES_NUM];*/
    unsigned short *TEST_GETALL_ARRAY;
    TEST_GETALL_ARRAY = (unsigned short *)calloc(SO_NODES_NUM, sizeof(unsigned short));
#endif

    /*Inizializzazione memoria condivisa per libro mastro*/
    shmIdMastro = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);

    /*Inizializzazione memoria condivisa per indice blocco libro mastro*/
    shmIdIndiceBlocco = shmget(IPC_PRIVATE, sizeof(int), 0600 | IPC_CREAT);
    /*Assegno indice iniziale all'id dei blocchi*/
    shmIndiceBloccoPtr = (int *)shmat(shmIdIndiceBlocco, NULL, 0);
    *(shmIndiceBloccoPtr) = 0;

/* SOLO PER TEST */
#if (ENABLE_TEST)
    printf("\nid libro mastro[%d]\n", shmIdMastro);
#endif

    /* Attach del libro mastro alla memoria condivisa*/
    shmLibroMastroPtr = (transazione *)shmat(shmIdMastro, NULL, 0);
    TEST_ERROR;

    /*inizializzazione memoria condivisa array ID coda di messaggi*/
    shmIdNodo = shmget(IPC_PRIVATE, SO_NODES_NUM * sizeof(int), 0600 | IPC_CREAT);
    shmArrayNodeMsgQueuePtr = (int *)shmat(shmIdNodo, NULL, 0); /*eseguo attach per riempire l'array di ID di code di messaggi*/

/* SOLO PER TEST */
#if (ENABLE_TEST)
    printf("\nLa shared memory dell'array contenente gli ID delle message queue è: %d\n", shmIdNodo);
#endif

    /*riempio l'arrayCodeID con gli ID delle code messaggi prima di entrare nel for per la creazione dei nodi*/
    for (i = 0; i < SO_NODES_NUM; i++)
    {
        msgQueueId = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
        *(shmArrayNodeMsgQueuePtr + i) = msgQueueId;
    }

/* SOLO PER TEST */
#if (ENABLE_TEST)
    for (i = 0; i < SO_NODES_NUM; i++)
    {
        printf("STAMPO TUTTI GLI ID DELLE CODE DI MESSAGGI CREATE\n");
        printf("ID coda n %d = %d\n", i, *(shmArrayNodeMsgQueuePtr + i));
    }
#endif

    /*Inizializzo set semafori per gestire invio transazioni da parte degli utenti*/
    semSetMsgQueueId = semget(IPC_PRIVATE, SO_NODES_NUM, 0600 | IPC_CREAT);
    TEST_ERROR;
    /*INIZIALIZZO L'ARRAY ATTRAVERSO UNA CHIAMATA DI CALLOC*/
    semMsgQueueValArr = (unsigned short *)calloc(SO_NODES_NUM, sizeof(unsigned short));
    /*Popolo array valore semafori */
    for (i = 0; i < SO_NODES_NUM; i++)
    {
        semMsgQueueValArr[i] = SO_TP_SIZE;
        printf("Valore indice %d = %d\n", i, semMsgQueueValArr[i]);
    }

    /*assegno array valori semafori alla union*/
    semMsgQueueUnion.array = semMsgQueueValArr;

    /*imposto valore semafori*/
    semctl(semSetMsgQueueId, 0, SETALL, semMsgQueueUnion);
    TEST_ERROR;

/*SOLO PER TEST*/
#if (ENABLE_TEST)
    TEST_GETALL.array = TEST_GETALL_ARRAY;
    semctl(semSetMsgQueueId, 0, GETALL, TEST_GETALL);
    for (i = 0; i < SO_NODES_NUM; i++)
    {
        printf("Valore semaforo ID : %d -> %d\n", i, TEST_GETALL.array[i]);
    }
#endif

    /*inizializzazione memoria condivisa array PID processi utente*/
    shmIdUtente = shmget(IPC_PRIVATE, SO_USERS_NUM * sizeof(int), 0600 | IPC_CREAT);
    shmArrayUsersPidPtr = (int *)shmat(shmIdUtente, NULL, 0);

    /*inizializzazione semaforo per garantire l'accesso alla memoria - TODO*/

/* SOLO PER TEST */
#if (ENABLE_TEST)
    printf("\nLa shared memory dell'array contenente gli ID delle message queue è: %d\n", shmIdUtente);
#endif

    /*FOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOORK*/

    semSyncStartId = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    TEST_ERROR
#if (ENABLE_TEST)
    printf("ID del semaforo di sincronizzazione dei processi -> %d", semSyncStartId);
#endif
    sops.sem_flg = 0; /*no flags*/
    sops.sem_num = 0; /*primo semaforo nel set*/
    sops.sem_op = SO_USERS_NUM + 1;
    semop(semSyncStartId, &sops, 1);
    TEST_ERROR

    /* creazione figli users con operazioni annesse */
    for (i = 0; i < SO_USERS_NUM; i++)
    {
        switch (childPid = fork())
        {

        case -1:
            fprintf(stderr, "%s, %d: Errore (%d) nella fork\n",
                    __FILE__, __LINE__, errno);
            exit(EXIT_FAILURE);
            break;
        case 0:
/* SOLO PER TEST*/
#if (ENABLE_TEST)
            printf("sono il figio: %d, in posizione: %d\n", getpid(), i);
#endif
            sops.sem_flg = 0;
            sops.sem_num = 0;
            sops.sem_op = -1;
            semop(semSyncStartId, &sops, 1);
            TEST_ERROR
/* SOLO PER TEST*/
#if (ENABLE_TEST)
            printf("%d is waiting for zero\n", i);
#endif
            sops.sem_flg = 0;
            sops.sem_num = 0;
            sops.sem_op = 0;
            semop(semSyncStartId, &sops, 1);
#if (ENABLE_TEST)
            printf("%d sono risvegliato\n", i);
#endif
            printf("SONO %d e Voglio creare la transazione e inviarla al nodo : %d\n", getpid(), getRandomUser(SO_USERS_NUM, getpid(), shmArrayUsersPidPtr));
            exit(EXIT_SUCCESS);
            break;

        default:
            /* riempimento array PID utenti nella memoria condivisa */
            *(shmArrayUsersPidPtr + i) = childPid;
            break;
        }
    }
    /* SOLO PER TEST*/
#if (ENABLE_TEST)
        printf("Sono #%d processo PARENT e risveglio i figli!\n", getpid());
#endif
    sops.sem_flg = 0;
    sops.sem_num = 0;
    sops.sem_op = -1;
    semop(semSyncStartId, &sops, 1);
#if (ENABLE_TEST)
    printf("Sono #%d processo PARENT e ho risvegliato i figli!\n", getpid());
#endif

    /*POSSIBILE IMPLEMENTAZIONE DELL'ATTESA DELLA TERMINAZIONE DEI FIGLI*/
    while((childPidWait = waitpid(-1, &childStatus, 0)) != -1)
    {
        if(WIFEXITED(childStatus))
        {
            printf("%d terminated with status %d - terminated by %d\n", childPidWait, WEXITSTATUS(childStatus), WTERMSIG(childStatus));
        }
        else if(WIFSIGNALED(childStatus))
        {
            printf("%d stopped by signal %d\n", childPidWait, WTERMSIG(childStatus));
        }
        else if(WCOREDUMP(childStatus))
        {
            printf("%d ha generato CORE DUMP\n", childPidWait);
        }
        else if(WIFSTOPPED(childStatus))
        {
            printf("%d stoppato da un SIGNAL di tipo %d\n", childPidWait, WSTOPSIG(childStatus));
        }
        else if(WIFCONTINUED(childStatus))
        {
            printf("%d e' stato risvegliato da un SIGNAL\n", childPid);
        }
        else
        {
            printf("ERRORE\n");
        }
    }

/* SOLO PER TEST*/
#if (ENABLE_TEST)
    for (i = 0; i < SO_USERS_NUM; i++)
    {
        printf("\nSTAMPO TUTTI GLI ID DEGLI UTENTI NELL'ARRAY CREATE\n");
        printf("PID utente %d = %d\n", i, *(shmArrayUsersPidPtr + i));
    }
#endif

    /*EFFETTUO LE DETACH, ALTRIMENTI LA OPERAZIONE NON AVRA' ALCUN EFFETTO*/
    shmdt(shmIndiceBloccoPtr);
    TEST_ERROR;
    shmdt(shmLibroMastroPtr);
    TEST_ERROR;
    shmdt(shmArrayNodeMsgQueuePtr);
    TEST_ERROR;
    shmdt(shmArrayUsersPidPtr);
    TEST_ERROR;
#if (ENABLE_TEST)
    printf("Id libro mastro - %d\nId Indice blocco - %d\nId Nodo - %d\nId Id utente - %d\n", shmIdMastro, shmIdIndiceBlocco, shmIdNodo, shmIdUtente);
#endif
    /*CHIUSURA SM*/
    shmctl(shmIdMastro, IPC_RMID, 0);
    TEST_ERROR;
    shmctl(shmIdIndiceBlocco, IPC_RMID, 0);
    TEST_ERROR;
    shmctl(shmIdNodo, IPC_RMID, 0);
    TEST_ERROR;
    shmctl(shmIdUtente, IPC_RMID, 0);
    TEST_ERROR;
    printf("QUA\n");

    sleep(3);

    printf("DOPO LA SLEEP\n");
    return 0;
}

int read_all_parameters()
{
    char buffer[256] = {};
    char *token;
    char *delimeter = "= ";
    int parsedValue;
    bzero(buffer, 256);
    FILE *configFile = fopen("parameters.txt", "r+");
    if (configFile != NULL)
    {
        // printf("File aperto!\n");
        while (fgets(buffer, 256, configFile) > 0)
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
            else if (strcmp(token, "SO_TP_SIZE") == 0)
            {
                SO_TP_SIZE = atoi(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_BLOCK_SIZE") == 0)
            {
                SO_BLOCK_SIZE = atoi(strtok(NULL, delimeter));
            }
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

            // while(token != NULL)
            // {
            //     printf("%s", token);
            //     token = strtok(NULL, "=");
            // }
        }
        // printf("\n");
        int closeResponse = fclose(configFile);
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
        printf("SO_TP_SIZE=%d\n", SO_TP_SIZE);
        printf("SO_BLOCK_SIZE=%d\n", SO_BLOCK_SIZE);
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

void alarmHandler(int signum)
{
    printf("L'ALLARM HANDLER!\n");
}

/*
La seguente funzione restituisce il PID dell'UTENTE scelto a caso a cui inviare la transazione.
Alcune assunzioni:
1. La memoria condivisa esiste ed e' POPOLATA
2. max rappresenta il valore di NUM_USERS che al momento della invocazione deve avere un valore valido.
*/
int getRandomUser(int max, int myPid, int *shmArrayUsersPidPtr)
{
    int userPidId;
    srand(myPid);
    
    do
    {
        userPidId = rand()%(max - 1);
    } while (*(shmArrayUsersPidPtr + userPidId) == myPid && *(shmArrayUsersPidPtr + userPidId) != -1);
    
    return *(shmArrayUsersPidPtr + userPidId);
}

/*
La seguente funzione attravera la shared memory, verificando se il contenuto della cella coincide con il valore passato come pidToRemove.
Alcune assunzioni:
1. La memoria condivisa esiste ed e' POPOLATA
2. numUsers rappresenta il valore di NUM_USERS che al momento della invocazione deve avere un valore valido.

DA CONCORDRE - assegnare al processo terminato -1 oppure un altro valore.
*/
void updateShmArrayUsersPid(int pidToRemove, int *shmArrayUsersPidPtr, int numUsers)
{
    /*ACCEDO IN MODO ESCLUSIVO ALLA SM - TODO*/
    for (int i = 0; i < numUsers; i++)
    {
        if(*(shmArrayUsersPidPtr + i) == pidToRemove)
        {
            *(shmArrayUsersPidPtr + i) = -1;
            break;
        }
    }
    /*RILASCIO LA RISORSA - TODO*/
}

/*
La seguente funzione restituisce il MQID della coda associata ad ogni NODO scelto a caso a cui inviare la transazione.
Alcune assunzioni:
1. La memoria condivisa esiste ed e' POPOLATA
2. max rappresenta il valore di NUM_NODES che al momento della invocazione deve avere un valore valido.
*/
int getRandomMsgQueueId(int max, int *shmArrayNodeMsgQueuePtr)
{
    srand(getpid());
    return *(shmArrayNodeMsgQueuePtr + rand()%(max - 1));
}

/*
La seguente funzione restituisce il bilancio dell'utente.
Alcune assunzioni:
1. La memoria condivisa esiste ed e' POPOLATA
2. max rappresenta il valore di SO_BLOCK_SIZE che al momento della invocazione deve avere un valore valido.
3. myPid del processo utente in esecuzione
*/
int getMyBilancio(int myPid, transazione *shmLibroMastroPtr, int *index, int max)
{
    int bilancio = 0;
    for(index; index < max; index++){
        if(((*(shmLibroMastroPtr + *index)).receiver == myPid || (*(shmLibroMastroPtr + *index)).sender == myPid) && (*(shmLibroMastroPtr + *index)).quantita != 0)
        {
            bilancio += (*(shmLibroMastroPtr + *index)).receiver;
        }
    }
    return bilancio;
}