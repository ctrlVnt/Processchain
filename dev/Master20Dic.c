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

/*indicare che è stata copiata dal codice visto a lezione...*/
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

#define ENABLE_TEST 1

typedef struct transazione
{                              /*inviata dal processo utente a uno dei processi nodo che la gestisce*/
    struct timespec timestamp; /*tempo transazione*/
    pid_t sender;              /*valore sender (getpid())*/
    pid_t receiver;            /*valore receiver -> valore preso da array processi UTENTE*/
    int quantita;              /*denaro inviato*/
    int reward;                /*denaro inviato al nodo*/
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

/*struct per invio messaggio*/
typedef struct mymsg
{
    long mtype;
    transazione transazione;
} message;

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
/*Estraggo un receiver casuale per l'invio della transazione*/
int getRandomUser(int max, int myPid, int *shmArrayUsersPidPtr);
/*funzione calcolo denaro da inviare*/
int calcoloDenaroInvio(int budget);

int main()
{

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
    int semLibroMastroId;
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
    int soRetry;
    int nodoScelto;
    transazione transazioneInvio;
    struct timespec timestampTransazione; /*struct per invio transazione*/
    int quantita;                         /*denaro totale da inviare*/
    int reward;                           /*reward nodo*/
    message messaggio;
    transazione *transactionPool;
    transazione *bloccoInvio;
    message myMsg;           /*contenitore messaggi ricevuti dal nodo*/
    int indiceTpCellaLibera; /*indica prima cella da sovrascrivere nella Tp*/
    int indiceTpInvioBlocco; /*indica prima cella con cui riempire il blocco*/
    /*********************/
/*SOLO PER TEST*/
#if (ENABLE_TEST)
    union semun TEST_GETALL;
    unsigned short *TEST_GETALL_ARRAY;
#endif

/* SOLO PER TEST ->però read_all_parameters() necessaria a prescindere */
#if (ENABLE_TEST)
    if (read_all_parameters() == -1)
    {
        fprintf(stderr, "Parsing failed!");
    }
#endif

    /*inizializzo array usando calloc con valore letto da file.txt causa std=c89 che non
      permette di dichiarare un array con lunghezza, cioè -> array[lunghezza]*/
#if (ENABLE_TEST)
    TEST_GETALL_ARRAY = calloc(SO_NODES_NUM, sizeof(int));
#endif
    /*alloco memoria array*/
    semMsgQueueValArr = calloc(SO_NODES_NUM, sizeof(int));

    /*Inizializzazione memoria condivisa per libro mastro*/
    shmIdMastro = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);

    /*Inizializzazione memoria condivisa per indice blocco libro mastro*/
    shmIdIndiceBlocco = shmget(IPC_PRIVATE, sizeof(int), 0600 | IPC_CREAT);
    /*Assegno indice iniziale all'id dei blocchi*/
    shmIndiceBloccoPtr = (int *)shmat(shmIdIndiceBlocco, NULL, 0);
    *(shmIndiceBloccoPtr) = 0;

    /*inizializzazione semaforo per scrittura nodi su libro mastro*/
    semLibroMastroId = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
    sops.sem_num = 0;
    sops.sem_flg = 0;
    sops.sem_op = 1;
    semop(semLibroMastroId, &sops, 1);

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
    /*Popolo array valore semafori */
    for (i = 0; i < SO_NODES_NUM; i++)
    {
        semMsgQueueValArr[i] = SO_TP_SIZE;
        /*test riempimento array -> si può togliere
        printf("Valore indice %d = %d\n", i, semMsgQueueValArr[i]);*/
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

/* SOLO PER TEST */
#if (ENABLE_TEST)
    printf("\nLa shared memory dell'array contenente gli ID delle message queue è: %d\n", shmIdUtente);
#endif

    /*creazione figli nodi con operazioni annesse*/
    for (i = 0; i < SO_NODES_NUM; i++)
    {

        childPid = fork();

        switch (childPid)
        {

        case -1:
            fprintf(stderr, "%s, %d: Errore (%d) nella fork\n",
                    __FILE__, __LINE__, errno);
            exit(EXIT_FAILURE);

        case 0:
            /*Attachment di ogni nodo alla memoria condivisa di OGNI coda di messaggi*/
            shmArrayNodeMsgQueuePtr = (int *)shmat(shmIdNodo, NULL, SHM_RDONLY);
            TEST_ERROR;
            /*attachment nodo i - esimo al libro mastro*/
            shmLibroMastroPtr = (transazione *)shmat(shmIdMastro, NULL, 0);
            TEST_ERROR;

            /*inizializzazione BLOCCO e TRANSLACTION POOL*/
            transactionPool = (transazione *)calloc(SO_TP_SIZE, sizeof(transazione));
            bloccoInvio = (transazione *)calloc(SO_BLOCK_SIZE, sizeof(transazione));
            indiceTpCellaLibera = 0;
            indiceTpInvioBlocco = 0;

            /*effettuo l'attach alla SM contente l'indice dove scrivire sul libro mastro*/
            shmIndiceBloccoPtr = (transazione *)shmat(shmIdIndiceBlocco, NULL, 0);
            while (1)
            {

                /*quando il semaforo è verde si riempie la Tp*/
                while (semctl(semMsgQueueValArr, i, GETVAL))
                { /*SOLUZIONE ALTERNATIVA: if con continue*/
                    /*riceve il messaggio nella propria coda di messaggi*/
                    msgrcv(*(shmArrayNodeMsgQueuePtr + i), &myMsg, sizeof(int), 0, 0);
                    TEST_ERROR;

                    transactionPool[indiceTpCellaLibera++] = myMsg.transazione;

                    if (indiceTpCellaLibera == SO_TP_SIZE)
                    {
                        indiceTpCellaLibera = 0;
                    }
                }
                /*semaforo è diventato zero e ora devo riempire il blocco*/
                for (i = 0; i < SO_BLOCK_SIZE - 1; i++)
                {
                    bloccoInvio[i] = transactionPool[indiceTpInvioBlocco++];
                    if (indiceTpInvioBlocco = SO_TP_SIZE)
                    {
                        indiceTpInvioBlocco = 0;
                    }
                }

                attesaNonAttiva();
                /*send al libr mastro e liberare risorse da semaforo*/
                sops.sem_flg = 0;
                sops.sem_num = 0;
                sops.sem_op = -1;
                semop(semLibroMastroId, &sops, 1);
                TEST_ERROR;

                /*TODO - GESTIONE riempimento del libro mastro gestito dal MASTER*/
                /*USARE HANDLER PER MANDARE SEGNALE AI NODI E TRASFORMARE MEMORIA CONDIVISA PER CODE DI MESSAGGI IN MATRICE CON I PID DEI NODI*/
                
                for(i = 0; i < SO_BLOCK_SIZE; i++)
                {
                    /*copia transazione I-esima da bloccoInvio nel Libro Mastro*/
                    shmLibroMastroPtr[*(shmIndiceBloccoPtr) + i] = bloccoInvio[i];
                }

                /*aggiornamento dell'indice*/
                *(shmIndiceBloccoPtr)++;

                /*rilascio delle risorse */
                sops.sem_flg = 0;
                sops.sem_num = 0;
                sops.sem_op = 1;
                semop(semLibroMastroId, &sops, 1);
                TEST_ERROR;
            }

            /*chiusura coda di messaggi*/
            msgctl(*(shmArrayNodeMsgQueuePtr + i), IPC_RMID, NULL);
            exit(EXIT_SUCCESS);
        default:
            break;
        }
    }

    /* creazione figli users con operazioni annesse */
    for (i = 0; i < SO_USERS_NUM; i++)
    {
        switch (childPid = fork())
        {

        case -1:
            fprintf(stderr, "%s, %d: Errore (%d) nella fork\n",
                    __FILE__, __LINE__, errno);
            exit(EXIT_FAILURE);

        case 0:
/* SOLO PER TEST*/
#if (ENABLE_TEST)
            printf("sono il figio: %d, in posizione: %d\n", getpid(), i);
#endif

            soRetry = SO_RETRY;

            while (soRetry)
            { /*il ciclo si ferma quando soRetry vale 0*/
                /*CALCOLO BILANCIO UTENTE*/
                /*da fare*/

                if (SO_BUDGET_INIT >= 2)
                {
                    /*seleziono il nodo a cui inviare -> EVENTUALE FUNZIONE PER CREAZIONE TRANSAZIONE*/
                    srand(getpid());
                    nodoScelto = rand() % SO_NODES_NUM;
                    /*Verifico disponibilità nodo*/
                    sops.sem_flg = IPC_NOWAIT;
                    sops.sem_num = nodoScelto;
                    sops.sem_op = -1;
                    semop(semSetMsgQueueId, &sops, 1);

                    if (errno == EAGAIN)
                    {
                        /*transaction pool piena*/
                        soRetry--;
                    }
                    else
                    {
                        /*la transaction pool ha spazio per la mia transazione*/
                        transazioneInvio.sender = getpid();
                        transazioneInvio.receiver = getRandomUser(SO_USERS_NUM, getpid(), shmArrayUsersPidPtr);
                        /*prendo il timestamp*/
                        clock_gettime(CLOCK_BOOTTIME, &timestampTransazione);
                        transazioneInvio.timestamp = timestampTransazione;
                        quantita = calcoloDenaroInvio(SO_BUDGET_INIT);
                        reward = (quantita * SO_REWARD) / 100;
                        if (reward == 0)
                        {
                            reward = 1;
                        }
                        transazioneInvio.reward = reward;
                        /*sottraggo denaro appena inviato al mio budget*/
                        SO_BUDGET_INIT -= quantita;
                        /*creo messaggio da inviare sulla coda di messaggi*/
                        messaggio.mtype = 1;
                        messaggio.transazione = transazioneInvio;
                        msgsnd(*(shmArrayNodeMsgQueuePtr + nodoScelto), &messaggio, sizeof(transazione), IPC_NOWAIT);
                        TEST_ERROR;

#if (ENABLE_TEST)
                        printf("%d ID CODA DI MESSAGGI A CUI INVIO %d\n", getpid(), *(shmArrayNodeMsgQueuePtr + nodoScelto));
#endif
                        /*todo attesa*/
                        /*release delle risorse da mettere nel nodo*/
                    }
                }
                else
                {
                    soRetry--;
                }
            }

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
    for (i = 0; i < SO_USERS_NUM; i++)
    {
        printf("\nSTAMPO TUTTI GLI ID DEGLI UTENTI NELL'ARRAY CREATE\n");
        printf("PID utente %d = %d\n", i, *(shmArrayUsersPidPtr + i));
    }
    exit(12345);
#endif

    /*CHIUSURA LIBRO MASTRO*/
    shmctl(shmIdMastro, IPC_RMID, 0);
    TEST_ERROR;
}

int read_all_parameters()
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
        userPidId = rand() % (max - 1);
    } while (*(shmArrayUsersPidPtr + userPidId) == myPid && *(shmArrayUsersPidPtr + userPidId) != -1);

    return *(shmArrayUsersPidPtr + userPidId);
}

int calcoloDenaroInvio(int budget)
{
    int res = 0;
    srand(getpid());
    while ((res = rand() % budget) <= 1)
    {
        res = rand() % budget;
    }
    return res;
}

void attesaNonAttiva()
{
    srand(getpid());
    long ntos = 1e9L;
    long diff = SO_MAX_TRANS_PROC_NSEC - SO_MIN_TRANS_PROC_NSEC;
    long attesa = rand() % diff + SO_MIN_TRANS_PROC_NSEC;
    int sec = attesa / ntos;
    long nsec = attesa - (sec * ntos);
#if (ENABLE_TEST)
    printf("SEC: %d\n", sec);
    printf("NSEC: %ld\n", nsec);
#endif
    struct timespec tempoDiAttesa;
    tempoDiAttesa.tv_sec = sec;
    tempoDiAttesa.tv_nsec = nsec;
    nanosleep(&tempoDiAttesa, NULL);
    /*TODO - MASCERARE IL SEGNALE SIGCONT*/
}
