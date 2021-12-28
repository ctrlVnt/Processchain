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

#define ENABLE_TEST 0

typedef struct transazione_ds
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

#define USER_OK 1
#define USER_KO 0

#define REWARD_SENDER -1

#define EXIT_PREMAT 2

/*struttura che tiene traccia dello stato dell'utente*/
typedef struct utente_ds
{
    int stato;
    int userPid;
    int bilancio;
} utente;

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

/*variabili globali*/
int nodo = 1; /*Negli utenti nodo == 0*/
              /****ID DELLE MEMORIE CONDIVISE****/
int shmIdMastro;
int shmIdNodo;
int shmIdUtente;
int shmIdIndiceBlocco;
/**********************************/
/*PUNTATORI ALLE STRUTTURE CONTENUTE ALL'INTERNO DELLE MEMORIE CONDIVISE*/
int *shmArrayNodeMsgQueuePtr;
utente *shmArrayUsersPidPtr;
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
/*unsigned short semMsgQueueValArr[SO_NODES_NUM]; dichiaro array di union semctl*/
/*ALTERNATIVA - allocazione dinamica con la calloc*/
unsigned short *semMsgQueueValArr;
/***************************************************************/
/*STRUTTURE PER GESTIONE DEI SEGNALI*/
struct sigaction sigactionForAlarmNew;
struct sigaction sigactionForAlarmOld;
sigset_t maskSetForAlarm;
sigset_t maskSetForChild;
sigset_t maskSetForChildOld;
struct sigaction act;
struct sigaction actOld;
/************************************/
/*VARIABILI AUSILIARI*/
int i;
int j;
int childPid;
int childPidWait;
int childStatus;
int soRetry;
int nodoScelto;
int tpPiena; /*variabile che indica lo stato di riempimento della transaction pool*/
transazione transazioneInvio;
transazione transazioneReward;
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
int *childNodePidArray; /*array contenente i pid dei processi NODI*/
/*contatore di processi vivi*/
int contatoreUtentiVivi;
/*indice per calcolo bilancio*/
int ultimoBloccoControllato = 0;
/*VARIABILE PER TEST HANDLER*/
int numerOrdineNodo;

/*Inizializza le variabili globali da un file di input TXT. */
int read_all_parameters();
/*Estraggo un receiver casuale per l'invio della transazione*/
int getRandomUser(int max, int myPid, utente *shmArrayUsersPidPtr);
/*funzione calcolo denaro da inviare*/
int calcoloDenaroInvio(int budget);
/*implementazione dell'attesa non attiva*/
void attesaNonAttiva(long, long);
/*handler*/
void signalHandler(int);
/*calcolo del bilancio*/
int calcoloBilancio();
/*Invio transazione*/
void sendTransaction();
/*algoritmo calcolo sleep*/
int sleepUtente();
int dormo;

int scelgoNodo();

int main()
{
    /*indice test vari -> da cancellare poi*/
    int a;
    tpPiena = 0;

/*SOLO PER TEST*/
#if (ENABLE_TEST)
    union semun TEST_GETALL;
    unsigned short *TEST_GETALL_ARRAY;
#endif

    if (read_all_parameters() == -1)
    {
        fprintf(stderr, "Parsing failed!");
    }
    contatoreUtentiVivi = SO_USERS_NUM; /*salvo in locale il numero di utenti vivi*/
	
	/*valore di sleep*/
	dormo = sleepUtente();
	printf("Valore sleep: %d\n", dormo);
	//sleep(5);
	
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
    printf("\nID libro mastro[%d]\n", shmIdMastro);
#endif
    /* Attach del libro mastro alla memoria condivisa*/
    shmLibroMastroPtr = (transazione *)shmat(shmIdMastro, NULL, 0);
    TEST_ERROR;

    /*inizializzazione memoria condivisa array ID coda di messaggi*/
    shmIdNodo = shmget(IPC_PRIVATE, SO_NODES_NUM * sizeof(int), 0600 | IPC_CREAT);
    shmArrayNodeMsgQueuePtr = (int *)shmat(shmIdNodo, NULL, 0); /*eseguo attach per riempire l'array di ID di code di messaggi*/
	
	
/*
#if (ENABLE_TEST)
    printf("\nLa shared memory dell'array contenente gli ID delle message queue è: %d\n", shmIdNodo);
#endif*/

    /*riempio l'arrayCodeID con gli ID delle code messaggi prima di entrare nel for per la creazione dei nodi*/
    for (i = 0; i < SO_NODES_NUM; i++)
    {
        msgQueueId = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
        *(shmArrayNodeMsgQueuePtr + i) = msgQueueId;
    }

/*
#if (ENABLE_TEST)
    for (i = 0; i < SO_NODES_NUM; i++)
    {
        printf("STAMPO TUTTI GLI ID DELLE CODE DI MESSAGGI CREATE\n");
        printf("ID coda n %d = %d\n", i, *(shmArrayNodeMsgQueuePtr + i));
    }
#endif*/

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

/*
#if (ENABLE_TEST)
    TEST_GETALL.array = TEST_GETALL_ARRAY;
    semctl(semSetMsgQueueId, 0, GETALL, TEST_GETALL);
    for (i = 0; i < SO_NODES_NUM; i++)
    {
        printf("Valore semaforo ID : %d -> %d\n", i, TEST_GETALL.array[i]);
    }
#endif*/

    /*inizializzazione memoria condivisa array PID processi utente*/
    shmIdUtente = shmget(IPC_PRIVATE, SO_USERS_NUM * sizeof(utente), 0600 | IPC_CREAT);
    shmArrayUsersPidPtr = (utente *)shmat(shmIdUtente, NULL, 0);

    /*inizializzo semaforo di sincronizzazione - POTREBBE FUNZIONARE ANCHE SENZA*/
    semSyncStartId = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
    sops.sem_num = 0;
    sops.sem_flg = 0;
    sops.sem_op = SO_NODES_NUM + 1;
    semop(semSyncStartId, &sops, 1);

/*
#if (ENABLE_TEST)
    printf("\nLa shared memory dell'array contenente gli ID delle message queue è: %d\n", shmIdUtente);
#endif*/
    /*inizializzazione array child NODE PID con budget di ognuno*/
    childNodePidArray = (int *)calloc(SO_NODES_NUM * 2, sizeof(int));
    TEST_ERROR;

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
            j = i;
            /*maschero alcuni segnali*/
            sigemptyset(&maskSetForChild);                                 /*inizializzo empty set, maschera VUOTA*/
            sigaddset(&maskSetForChild, SIGINT);                           /* aggiungo SIGINT alla mashera*/
            sigaddset(&maskSetForChild, SIGSTOP);                          /*aggiungo SIGSTOP alla maschera*/
            sigprocmask(SIG_BLOCK, &maskSetForChild, &maskSetForChildOld); /*blocco i seganli indicati sopra*/
            act.sa_mask = maskSetForChild;                                 /*aggiungo la maschera all'act(struttura)*/
            act.sa_handler = signalHandler;
            act.sa_flags = 0;
            sigaction(SIGUSR1, &act, &actOld);

            /*avviso il PARENT*/
            sops.sem_flg = 0;
            sops.sem_num = 0;
            sops.sem_op = -1;
            semop(semSyncStartId, &sops, 1);

            /*in attesa di zero*/
            sops.sem_flg = 0;
            sops.sem_num = 0;
            sops.sem_op = 0;
            semop(semSyncStartId, &sops, 1);
			printf("---------IO NODO MI SONO SVEGLIATO-----------\n");
            /*POSSO INIZIARE A LAVORARE*/
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
            shmIndiceBloccoPtr = (int *)shmat(shmIdIndiceBlocco, NULL, 0);

            /*transazione reward*/
            transazioneReward.sender = REWARD_SENDER;
            transazioneReward.reward = 0;
            transazioneReward.quantita = 0;
            transazioneReward.receiver = getpid();
            while (nodo)
            {
                /*quando il semaforo è verde si riempie la Tp*/
                /*esplicitando la condizione di uscita il valore del semaforo viene beccato dal while*/
                /*causa scheduler forse converrebbe aggiungere una condizione nel while es -> tpSize == SO_TP_SIZE
                  con tpSize++ ad ogni inserimento nella transaction pool*/
                while (tpPiena < SO_TP_SIZE)
                {
                    /*printf("valore semaforo j :%d nodo: %d\n", semctl(semSetMsgQueueId, j, GETVAL), getpid());*/
                    TEST_ERROR;
                    /*SOLUZIONE ALTERNATIVA: if con continue*/
                    /*riceve il messaggio nella propria coda di messaggi*/
                    printf("------------PROVO A FARE LA RECEIVE-------------\n");
                    int a = msgrcv(shmArrayNodeMsgQueuePtr[j], &myMsg, sizeof(transazione), 0, 0);
                    printf("---------------[%d] con coda %d con ID :%d -> leggo %d----------------\n", getpid(), j, shmArrayNodeMsgQueuePtr[j], a);
                   
/*#if (ENABLE_TEST)	
                    controllo che nessuna transazione letta venga sovrascritta in qualche modo
                    printf("Valore quantita transazione appena letta: %d\n", myMsg.transazione.quantita);
#endif*/
                    TEST_ERROR;
                    tpPiena++;
                    /*printf("mtype: %ld\n", myMsg.mtype);*/

                    transactionPool[indiceTpCellaLibera++] = myMsg.transazione;

                    if (indiceTpCellaLibera == SO_TP_SIZE)
                    {
                        indiceTpCellaLibera = 0;
                    }
                }
/*esegue la stampa*/
#if (ENABLE_TEST)
                printf("\nSTAMPO STATO TRANSLATION POOL\n");
                /*per un test corretto non devo avere valori quantita == 0 -> quantita che viene scartata durante la creazione
                  della transazione e di conseguenza mai inviata*/
                for (a = 0; a < SO_TP_SIZE; a++)
                {
                    printf("[%d]TP indice: %d con quantita: %d\n", getpid(), a, transactionPool[a].quantita);
                }
                printf("\nCOMINCIO RIEMPIMENTO BLOCCO\n");
#endif
                /*semaforo è diventato zero e ora devo riempire il blocco*/
                for (i = 0; i < SO_BLOCK_SIZE - 1; i++)
                {
                    bloccoInvio[i] = transactionPool[indiceTpInvioBlocco++];
/*#if (ENABLE_TEST)
                    printf("Valore indiceTpInvioBlocco: %d\n", indiceTpInvioBlocco);
                    printf("Ho inserito in pos: %d, una transazione con quantita': %d\n", i, bloccoInvio[i].quantita);
#endif*/
                    /*transazione di reward*/
                    transazioneReward.quantita += bloccoInvio[i].reward;

                    if (indiceTpInvioBlocco == SO_TP_SIZE) /*mancava un ... di uguale e riempiva sempre con la stessa transazione*/
                    {
                        indiceTpInvioBlocco = 0;
                    }
                }
                /*aggiorno il timestamp della transazione di reward*/
                clock_gettime(CLOCK_BOOTTIME, &transazioneReward.timestamp);
                /*aggiungo l'ultima transazione nel blocco, quella di reward*/
                bloccoInvio[SO_BLOCK_SIZE - 1] = transazioneReward;
                transazioneReward.quantita = 0;

/*Controllo che il riempimento del blocco sia avvenuto con successo*/
#if (ENABLE_TEST)
                printf("\nTerminato il riempimento del blocco\n");
                for (a = 0; a < SO_BLOCK_SIZE; a++)
                {
                    printf("[%d]Ho inserito in pos: %d, una transazione con quantita': %d\n",getpid(), a, bloccoInvio[a].quantita);
                }
#endif
                attesaNonAttiva(SO_MIN_TRANS_PROC_NSEC, SO_MAX_TRANS_PROC_NSEC);
                /*send al libr mastro e liberare risorse da semaforo*/
                sops.sem_flg = 0;
                sops.sem_num = 0;
                sops.sem_op = -1;
                semop(semLibroMastroId, &sops, 1);  
                TEST_ERROR;
                int indiceLocale = *(shmIndiceBloccoPtr);
                /*TODO - GESTIONE riempimento del libro mastro gestito dal MASTER*/
                /*USARE HANDLER PER MANDARE SEGNALE AI NODI E TRASFORMARE MEMORIA CONDIVISA PER CODE DI MESSAGGI IN MATRICE CON I PID DEI NODI*/

                /*controllo id libro mastro per verificare se posso scrivere un blocco di transazioni, altrimenti sospendo il processo nodo*/
               if (*(shmIndiceBloccoPtr) == SO_REGISTRY_SIZE)
                {

#if (ENABLE_TEST)
                    printf("CAPACITÀ MASSIMA LIBRO MASTRO RAGGIUNTA\n");
#endif

                    sigsuspend(&maskSetForChild);
                }

                /*qui si trovava il for per la copia nel libro mastro*/

/*Controllo di aver riempito il libro mastro in maniera corretta*/
/*
#if (ENABLE_TEST)
                for (a = 0; a < SO_BLOCK_SIZE; a++)
                {
                    printf("LIBRO MASTRO in pos: %d con quantita: %d\n", a, shmLibroMastroPtr[*(shmIndiceBloccoPtr)*SO_BLOCK_SIZE + a].quantita);
                }
#endif/*

                /*aggiornamento dell'indice*/
                *(shmIndiceBloccoPtr) += 1;
/*#if (ENABLE_TEST)
                printf("Valore indice blocco: %d\n", *(shmIndiceBloccoPtr));
#endif*/

                /*rilascio delle risorse */
                sops.sem_flg = 0;
                sops.sem_num = 0;
                sops.sem_op = 1;
                semop(semLibroMastroId, &sops, 1);
                TEST_ERROR;

                for (i = 0; i < SO_BLOCK_SIZE; i++)
                {
                    /*copia transazione I-esima da bloccoInvio nel Libro Mastro*/
                    // shmLibroMastroPtr[*(shmIndiceBloccoPtr) + i] = bloccoInvio[i];
                    /*formula che aveva spiegato DE PIERRO per il riempimento della matrice*/
                    /* *(shmLibroMastroPtr + (*(shmIndiceBloccoPtr)*SO_BLOCK_SIZE + i)) = bloccoInvio[i];*/
                    shmLibroMastroPtr[indiceLocale*SO_BLOCK_SIZE + i] = bloccoInvio[i];
                }
                indiceLocale = 0;

                /*incremento il valore del semaforo, cosi' sblocco la scrittura sulla coda di messaggi da parte degli utenti*/
                sops.sem_flg = 0;
                sops.sem_num = j;
                sops.sem_op = (SO_BLOCK_SIZE - 1);
                semop(semSetMsgQueueId, &sops, 1);
                TEST_ERROR;
                tpPiena -= SO_BLOCK_SIZE - 1;

                printf("valore semaforo j :%d nodo: %d\n", semctl(semSetMsgQueueId, j, GETVAL), getpid());

#if (ENABLE_TEST)
                printf("1.TP piena: %d\n", tpPiena);
#endif
            }

            /*chiusura coda di messaggi*/
            /*STAMPO transazioni non spedite ma salvate nella TP -> lo faccio nel handler quindi da togliere*/
            /* printf("%d di transazioni rimasti no gestite nella TP", SO_TP_SIZE - semctl(semSetMsgQueueId, j, GETVAL));*/
            TEST_ERROR;
            msgctl(*(shmArrayNodeMsgQueuePtr + j), IPC_RMID, NULL);
            exit(EXIT_SUCCESS);
        default:
            childNodePidArray[i] = childPid;
            childNodePidArray[SO_NODES_NUM + i] = 0;
			
#if (ENABLE_TEST)
            printf("Child NODE %d e' stato creato e memorizzato nella struttura\n", childNodePidArray[i]);
#endif
            break;
        }
    }

    /*risveglio i nodi*/
    sops.sem_flg = 0;
    sops.sem_num = 0;
    sops.sem_op = -1;
    semop(semSyncStartId, &sops, 1);
    TEST_ERROR;

    /*sincronizzo i figli*/
  /*  sops.sem_num = 0;
    sops.sem_flg = 0;
    sops.sem_op = SO_USERS_NUM + 1;
    semop(semSyncStartId, &sops, 1);*/

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
            /*variabile da eliminare quando suddividiamo in moduli*/
            nodo = 0;
            /*maschero alcuni segnali*/
            sigemptyset(&maskSetForChild);                                 /*inizializzo empty set, maschera VUOTA*/
            sigaddset(&maskSetForChild, SIGINT);                           /*aggiungo SIGINT alla mashera*/
            sigaddset(&maskSetForChild, SIGSTOP);                          /*aggiungo SIGSTOP alla maschera*/
            sigprocmask(SIG_BLOCK, &maskSetForChild, &maskSetForChildOld); /*blocco i seganli indicati sopra*/
            act.sa_mask = maskSetForChild;                                 /*aggiungo la maschera all'act(struttura)*/
            act.sa_handler = signalHandler;
            act.sa_flags = 0;
            sigaction(SIGUSR2, &act, &actOld);
            /*  sigaction(SIGUSR1, &act, &actOld);*/

            /*avviso il PARENT*/
         /*   sops.sem_flg = 0;
            sops.sem_num = 0;
            sops.sem_op = -1;
            semop(semSyncStartId, &sops, 1);*/

            /*in attesa di zero*/
         /*   sops.sem_flg = 0;
            sops.sem_num = 0;
            sops.sem_op = 0;
            semop(semSyncStartId, &sops, 1);*/
/* SOLO PER TEST*/
/*#if (ENABLE_TEST)
            printf("sono il figio: %d, in posizione: %d\n", getpid(), i);
#endif*/

            soRetry = SO_RETRY;

            numerOrdineNodo = i;

            while (soRetry != 0)
            {   /*il ciclo si ferma quando soRetry vale 0*/
            sendTransaction();
            }
#if(ENABLE_TEST)            /*AGGIORNARE STATO UTENTE NELL'ARRAY QUANDO MUORE*/
            printf("3.STO MORENDO...AIUTOOOO.....\n");
#endif
            shmArrayUsersPidPtr[i].stato = USER_KO;
            exit(EXIT_PREMAT);
            break;

        default:
            /* riempimento array PID utenti nella memoria condivisa */
            shmArrayUsersPidPtr[i].stato = USER_OK;
            shmArrayUsersPidPtr[i].userPid = childPid;
            shmArrayUsersPidPtr[i].bilancio = SO_BUDGET_INIT;
            /*aspetto per la creazione*/
            struct timespec tempo1;
            tempo1.tv_sec = 0;
            tempo1.tv_nsec = 250000;
            nanosleep(&tempo1, NULL);
            break;
        }
    }

    /*risveglio gli utenti*/
    /*sops.sem_flg = 0;
    sops.sem_num = 0;
    sops.sem_op = -1;
    semop(semSyncStartId, &sops, 1);
    TEST_ERROR;*/

    /*DA QUI IN POI HO TUTTI GLI UTENTI E TUTTI I NODI ATTIVI*/

    /*PARTE EFFETTIVA DEL MASTER*/

/*
#if (ENABLE_TEST)
    for (i = 0; i < SO_USERS_NUM; i++)
    {
        printf("\nSTAMPO TUTTI GLI ID DEGLI UTENTI NELL'ARRAY CREATE\n");
        printf("PID utente %d = %d\n", i, shmArrayUsersPidPtr[i].userPid);
    }
#endif*/

    sleep(3);
    /*CONTROLLO indice libro mastro, se == SO_REGISTRY_SIZE allora mando segnale per terminare tutto */
    /*introduco controllo periodico capacità libro mastro, qui dovremo mettere anche le varie stampe intermedie*/
    int master = 1;
    int status = 0;
    int pidmax;
    int max;
    utente pidmin;
    utente fantoccio;
    int b;
    int c = 0;
    pidmin = shmArrayUsersPidPtr[1];
    fantoccio = shmArrayUsersPidPtr[1];
    int nodeMax = 0;
    int nodeMin = 0;
    int y = 0;
    int j = 0;
    while (master)
    {
        sleep(1);
#if(ENABLE_TEST == 0)
        
        max = 0;
        printf("\nPROCESSI UTENTE ATTIVI: %d\n", contatoreUtentiVivi);
        printf("PROCESSI NODO ATTIVI: %d\n", SO_NODES_NUM);
        
        printf("\n");
        if(SO_USERS_NUM < 20){
            for (b = 0; b < SO_USERS_NUM; b++){
                printf("P.UTENTE [%d] ha bilancio di: %d\n", shmArrayUsersPidPtr[b].userPid, shmArrayUsersPidPtr[b].bilancio);
            }
        }else{
            if(pidmin.bilancio <= 2){
                    pidmin = fantoccio;
                }
            for (b = 0; b < SO_USERS_NUM; b++){
                if(shmArrayUsersPidPtr[b].bilancio > max){
                    pidmax = shmArrayUsersPidPtr[b].userPid;
                    max = shmArrayUsersPidPtr[b].bilancio;                    
                }
                
                if(shmArrayUsersPidPtr[b].bilancio <= pidmin.bilancio && shmArrayUsersPidPtr[b].stato != USER_KO){ /*se metto > 2 restituisce altro*/
                    pidmin = shmArrayUsersPidPtr[b];
                }
            }
            printf("P.UTENTE MAX [%d] ha bilancio di: %d\n", pidmax, max);
            printf("P.UTENTE MIN [%d] ha bilancio di: %d\n", pidmin.userPid, pidmin.bilancio);
        }
        if (SO_NODES_NUM < 20){
            for (c; c < *(shmIndiceBloccoPtr); c++){
                for(b = 0; b < SO_NODES_NUM; b++){
                    if(childNodePidArray[b] == shmLibroMastroPtr[c * SO_BLOCK_SIZE + SO_BLOCK_SIZE - 1].receiver){
                        childNodePidArray[b + SO_NODES_NUM] += shmLibroMastroPtr[c * SO_BLOCK_SIZE + SO_BLOCK_SIZE - 1].quantita;
                    }
                }
            }
            for(b = 0; b < SO_NODES_NUM; b++){
                    printf("P.NODO [%d] budget: %d\n", childNodePidArray[b], childNodePidArray[b + SO_NODES_NUM]);
                }
        }else{
            for (c; c < *(shmIndiceBloccoPtr); c++){
                for(b = 0; b < SO_NODES_NUM; b++){
                    if(childNodePidArray[b] == shmLibroMastroPtr[c * SO_BLOCK_SIZE + SO_BLOCK_SIZE - 1].receiver){
                        childNodePidArray[b + SO_NODES_NUM] += shmLibroMastroPtr[c * SO_BLOCK_SIZE + SO_BLOCK_SIZE - 1].quantita;
                    }
                }
            }
            for(b = 0; b < SO_NODES_NUM; b++){
                if (childNodePidArray[b + SO_NODES_NUM] > nodeMax){
                    nodeMax = childNodePidArray[b + SO_NODES_NUM];
                    y = b;
                }
                if (childNodePidArray[b + SO_NODES_NUM] < nodeMin){
                    nodeMin = childNodePidArray[b + SO_NODES_NUM];
                    j = b;
                }
            }                     
            printf("P.NODO MAX [%d] ha bilancio di: %d\n", childNodePidArray[y], nodeMax);
            printf("P.NODO MIN [%d] ha bilancio di: %d\n", childNodePidArray[j], nodeMin);
        }
        printf("\n");
#endif

        while ((childPid = waitpid(-1, &status, WNOHANG)) != 0)
        {
            TEST_ERROR;
            if (WIFEXITED(status))
            {
                if (WEXITSTATUS(status) == EXIT_PREMAT)
                {
                    printf("%d terminato prematuramente con status %d\n", childPid, WEXITSTATUS(status));
                }
                else
                {
                    printf("%d terminato con status %d\n", childPid, WEXITSTATUS(status));
                }
                contatoreUtentiVivi--;
            }
        }
        /*
#if(ENABLE_TEST)
        printf("--------------------------CONTATORE UTENTI VIVI:%d-------------------------------\n", contatoreUtentiVivi);
#endif*/
        /*STAMPO STATO DEL REGISTRO*/
        /*inserire condizion utentiInVita == 0 + alarm(SO_SIM_SEC) da gestire con un handler*/
        if (*(shmIndiceBloccoPtr) == SO_REGISTRY_SIZE || contatoreUtentiVivi <= 1)
        {
            /*STAMPO CONDIZIONI DI USCITA*/

            printf("valore shmIndiceBlocco %d\n", *(shmIndiceBloccoPtr));
            /*stampo stato libro mastro dopo l'esecuzione*/
            for (b = 0; b < *(shmIndiceBloccoPtr); b++)
            {
                printf("|");
                for (c = 0; c < SO_BLOCK_SIZE; c++)
                {
                    if(c == SO_BLOCK_SIZE-1){
                        printf("%05d| PID: %d",shmLibroMastroPtr[b * SO_BLOCK_SIZE + c].quantita, shmLibroMastroPtr[b * SO_BLOCK_SIZE + c].receiver);
                    }else{
                        printf("%05d|",shmLibroMastroPtr[b * SO_BLOCK_SIZE + c].quantita);
                    }
                    /*printf("%03d ----> %d|",shmLibroMastroPtr[b * SO_BLOCK_SIZE + c].quantita, shmLibroMastroPtr[b * SO_BLOCK_SIZE + c].receiver);*/
                    /*printf("%03d|",shmLibroMastroPtr[b * SO_BLOCK_SIZE + c].quantita);*/
                }
                printf("\n");
            }

            master = 0;

            for (i = 0; i < SO_NODES_NUM; i++)
            {
                kill(childNodePidArray[i], SIGUSR1);
                TEST_ERROR;
            }

            for (i = 0; i < SO_USERS_NUM; i++)
            {
                if (shmArrayUsersPidPtr[i].stato != USER_KO)
                {
                    kill(shmArrayUsersPidPtr[i].userPid, SIGUSR1);
                    TEST_ERROR;
                }
            }
        }
    }
    /*sleep(20);*/
    /*qui c'era stampa del registro*/

    /*CHIUSURA LIBRO MASTRO*/
    /*dealloco tutte le strutture*/
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
int getRandomUser(int max, int myPid, utente *shmArrayUsersPidPtr)
{
    int userPidId;
    /*SPERIMENTALE*/
    struct timespec tempo;
    clock_gettime(CLOCK_REALTIME, &tempo);
    /*FINE SPERIMENTALE*/

    srand(tempo.tv_nsec); /*il pid rimane sempre quello per ogni processo*/
    /*
    do
    {
        userPidId = rand() % max;
    } while (shmArrayUsersPidPtr[userPidId].userPid == myPid && shmArrayUsersPidPtr[userPidId].stato != USER_KO);
    */
    userPidId = (rand() % (max - 0 + 1)) + 0;
   
    return shmArrayUsersPidPtr[userPidId].userPid;
}

int calcoloDenaroInvio(int budget)
{
    int res = 0;
    struct timespec tempo;
    clock_gettime(CLOCK_REALTIME, &tempo);
    srand(tempo.tv_nsec);
    /*srand(getpid);*/
    /*suppongo che la transazione minima abbia quantita == 1 e reward == 1*/
    do
    {
        res = (rand() % (budget));

        if (res == 1)
        {
            res++;
        }
    } while (res < 1);
    
    return res;
}

/*
La seguente funzione esegue l'attesa NON attiva.
Si assume che nsecMin < nsecMax
*/
void attesaNonAttiva(long nsecMin, long nsecMax)
{
    srand(getpid());
    long ntos = 1e9L;
    long diff = nsecMax - nsecMin;
    long attesa = rand() % diff + nsecMin;
    int sec = attesa / ntos;
    long nsec = attesa - (sec * ntos);
/*#if (ENABLE_TEST)
    printf("SEC: %d\n", sec);
    printf("NSEC: %ld\n", nsec);
#endif*/
    struct timespec tempoDiAttesa;
    tempoDiAttesa.tv_sec = sec;
    tempoDiAttesa.tv_nsec = nsec;
    nanosleep(&tempoDiAttesa, NULL);
    /*TODO - MASCERARE IL SEGNALE SIGCONT*/
}

/*
Handler d'esempio
*/
void signalHandler(int sigNum)
{
    switch (sigNum)
    {
    case SIGUSR1:
#if(ENABLE_TEST == 1)
        printf("SONO HANDLER DEL NODO!\n");
        printf("%d di transazioni rimasti no gestite nella TP\n", SO_TP_SIZE - semctl(semSetMsgQueueId, j, GETVAL));
#endif
        /*eseguo detach shared memory nel nodo -> da implementare nella suddivisione in moduli*/
        shmdt(shmLibroMastroPtr);
        shmdt(shmIndiceBloccoPtr);
        shmdt(shmArrayNodeMsgQueuePtr);
        shmdt(shmArrayUsersPidPtr);
        exit(EXIT_SUCCESS);
        break;
    case SIGUSR2:
        /* IPOTIZZATO INVIO CON SEGNALE */
                    
                    nodoScelto = scelgoNodo();
                    
                    sops.sem_flg = IPC_NOWAIT;
                    sops.sem_num = nodoScelto;
                    sops.sem_op = -1;
                    semop(semSetMsgQueueId, &sops, 1);

                    if (errno != EAGAIN)
                    {
                        transazioneInvio.sender = getpid();
                        transazioneInvio.receiver = getRandomUser(SO_USERS_NUM, getpid(), shmArrayUsersPidPtr);
                        
                        clock_gettime(CLOCK_BOOTTIME, &timestampTransazione);
                        
                        transazioneInvio.timestamp = timestampTransazione;
                        quantita = calcoloDenaroInvio(shmArrayUsersPidPtr[i].bilancio);
                        
                        reward = (quantita * SO_REWARD) / 100;
                        if (reward == 0)
                        {
                            reward = 1;
                        }
                        transazioneInvio.reward = reward;
                        transazioneInvio.quantita = 707070;
                        
                        messaggio.mtype = getpid();
                        messaggio.transazione = transazioneInvio;
                        msgsnd(shmArrayNodeMsgQueuePtr[nodoScelto], &messaggio, sizeof(transazione), 0); 
                        TEST_ERROR;
                        soRetry = SO_RETRY;
                        
                        attesaNonAttiva(SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC);

                        shmArrayUsersPidPtr[numerOrdineNodo].bilancio = calcoloBilancio(shmArrayUsersPidPtr[numerOrdineNodo]);
                        printf("\n[%d] Ho inviato una transazione con SIGUSR2 A[%d] con -------> %d\n", getpid(), nodoScelto, messaggio.transazione.quantita);
                    }
                        
#if(ENABLE_TEST)
        printf("SONO HANDLER DELL'UTENTE!\n");
#endif
        break;
    default:
        break;
    }
    printf("Signal ricevuto, cosi' l'ho gestito OK?!\n");
}

/*
handler processo utente termina prematuramente
*/
void userTermPremat(int sigNum)
{
    printf("Processo utente terminato!\n");
    contatoreUtentiVivi--;
}

int calcoloBilancio(utente bilanciato){
    int u;
    for (ultimoBloccoControllato; ultimoBloccoControllato < *(shmIndiceBloccoPtr); ultimoBloccoControllato++){
        for(u = 0; u < SO_BLOCK_SIZE - 1; u++){
            if(getpid() == shmLibroMastroPtr[ultimoBloccoControllato * SO_BLOCK_SIZE + u].receiver){
                bilanciato.bilancio += shmLibroMastroPtr[ultimoBloccoControllato * SO_BLOCK_SIZE + u].quantita;
            }
        }
    }

    return bilanciato.bilancio;
}

int scelgoNodo(){
    int nodoScelto;
    struct timespec tempo;
    clock_gettime(CLOCK_REALTIME, &tempo);
    srand(tempo.tv_nsec + 1);
    nodoScelto = (rand() % ((SO_NODES_NUM - 1) - 0 + 1)) + 0;
    return nodoScelto;
}

/*TODO
1.modificare la funzione di attesa non attiva da usare sia per i nodi che per gli utenti - V
2.funzione che calcola il budget dell'utente prima di inviare alcuna transazione - V
3.Il master deve stampare ogni secondo il bilancio di tutti i nodi presenti sul REGSTRO - V
-si potrebbe aggiungere un campo alla struct dove aggiorniamo il budget corrente
4.Mascherare i segnali prima e smascherarli subito dopo dell'attesa - V
5.Aggiungere tempi di attesa - V
6.Creare transazione di reward del nodo - V
7.Aggiornare stato utente nel rispettivo array quando muore - V
8.Bisogna tenere conto degli utenti vivi - V
9.Impostare segnale invio transazione spontanea - X
*/

void sendTransaction(){
      shmArrayUsersPidPtr[i].bilancio = calcoloBilancio(shmArrayUsersPidPtr[i]);
    if (shmArrayUsersPidPtr[i].bilancio >= 2)
                {	
                	
                    /*printf("Cerco di riservare il semaforo\n");*/
                    /*seleziono il nodo a cui inviare -> EVENTUALE FUNZIONE PER CREAZIONE TRANSAZIONE*/
                    
                    /*srand(time(NULL));
                    nodoScelto = rand() % SO_NODES_NUM;*/

                    nodoScelto = scelgoNodo();
                    
                    /*Verifico disponibilità nodo*/
                    sops.sem_flg = IPC_NOWAIT;
                    sops.sem_num = nodoScelto;
                    sops.sem_op = -1;
                    semop(semSetMsgQueueId, &sops, 1);

                    if (errno == EAGAIN)
                    {
#if(ENABLE_TEST == 1)        /*transaction pool piena*/
                        printf("2. soRetry: %d pid: %d -> SEMAFORO OCCUPATO\n", soRetry, getpid());
#endif
                        soRetry--;
                        sleep(dormo);
                    }
                    else
                    {
                        /*printf("Semaforo libero -> L'ho riservato\n");*/
                        /*printf("4.%d sto creando una transazione\n", getpid());*/
                        /*la transaction pool ha spazio per la mia transazione*/
                        transazioneInvio.sender = getpid();
                        transazioneInvio.receiver = getRandomUser(SO_USERS_NUM - 1, getpid(), shmArrayUsersPidPtr);
                        /*prendo il timestamp*/
                        clock_gettime(CLOCK_BOOTTIME, &timestampTransazione);
                        /*printf("Ho impostato il timestamp%d\n", getpid());*/
                        transazioneInvio.timestamp = timestampTransazione;
                        quantita = calcoloDenaroInvio(shmArrayUsersPidPtr[i].bilancio);
                        printf("7.%d scelto la quantità totale da inviare: %d\n", getpid(), quantita);
#if(ENABLE_TEST)
                        printf("[%d] Invio %d\n", getpid(), quantita);
#endif
                        reward = (quantita * SO_REWARD) / 100;
                        if (reward == 0)
                        {
                            reward = 1;
                        }
                        transazioneInvio.reward = reward;
                        transazioneInvio.quantita = quantita - reward;
                        shmArrayUsersPidPtr[i].bilancio -= quantita;
#if(ENABLE_TEST)
                        printf("[%d] INVIO %d SOLDI A %d\n", getpid(), transazioneInvio.quantita, transazioneInvio.receiver);
#endif
                        /*creo messaggio da inviare sulla coda di messaggi*/
                        messaggio.mtype = getpid();
                        messaggio.transazione = transazioneInvio;
                        msgsnd(shmArrayNodeMsgQueuePtr[nodoScelto], &messaggio, sizeof(transazione), 0); /*prima era IPC_NOWAIT*/
                        TEST_ERROR;
                        soRetry = SO_RETRY;
                        /*attesa non attiva*/
                        /*maschero - SIGUSR2*/
                        /*printf("Transazione creata,ATTESA NON ATTIVA INIZIATA %d\n", getpid());*/
                        attesaNonAttiva(SO_MIN_TRANS_GEN_NSEC, SO_MAX_TRANS_GEN_NSEC);
                        /*printf("Transazione creata,ATTESA NON ATTIVA FINITA %d\n", getpid());*/
                        /*smaschera - SIGINT, SIGSTOP*/

/*#if (ENABLE_TEST)
                        printf("%d ID CODA DI MESSAGGI A CUI INVIO %d VALORE: %d\n", getpid(), *(shmArrayNodeMsgQueuePtr + nodoScelto), transazioneInvio.quantita);
#endif*/
                        /*Test rilascio semaforo
                        sops.sem_flg = IPC_NOWAIT;
                        sops.sem_num = nodoScelto;
                        sops.sem_op = 1;
                        semop(semSetMsgQueueId, &sops, 1);*/
                        /*todo attesa*/
                        /*release delle risorse da mettere nel nodo*/
                    }
                }
                else
                {
#if(ENABLE_TEST==1)
                printf("2.5. soRetry: %d pid: %d -> BUDGET INSUFFICIENTE DI: %d\n", soRetry, getpid(), shmArrayUsersPidPtr[i].bilancio);
#endif
                    soRetry--;
                   sleep(dormo);
                }

                shmArrayUsersPidPtr[i].bilancio = calcoloBilancio(shmArrayUsersPidPtr[i]);
}

int sleepUtente(){
	return (SO_USERS_NUM /(SO_NODES_NUM*(SO_TP_SIZE-(SO_BLOCK_SIZE - 1))))*2;  /*FORSE più ragionevole*/
}
              

