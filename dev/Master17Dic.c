#define _GNU_SOURCE
#include <stdio.h>	//input/output
#include <stdlib.h> //effettuare exit()
#include <errno.h>	//prelevare valore errno
#include <string.h> //operazioni stringhe

#include <sys/shm.h>   //memoria condivisa
#include <sys/ipc.h>   //recuperare i flag delle strutture system V
#include <sys/types.h> //per compatiblità
#include <sys/sem.h> /*operazione sui semafori*/
#include <sys/msg.h> /*operazione sulle code di messaggi*/

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

#define ENABLE_TEST 1

typedef struct transazione
{							   //inviata dal processo utente a uno dei processi nodo che la gestisce
	struct timespec timestamp; //tempo transazione
	pid_t sender;			   //valore sender (getpid())
	pid_t receiver;			   //valore receiver -> valore preso da array processi UTENTE
	int quantita;			   //denaro inviato
	int reward;				   //denaro inviato al nodo
} transazione;

union semun{
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

int main()
{
    /* SOLO PER TEST */
		#if(ENABLE_TEST)
		if (read_all_parameters() == -1){
			fprintf(stderr, "Parsing failed!");
		}
		#endif

	int shmIdMastro;
	int shmIdNodo;
	int shmIdUtente;
	int *arrayNodeMsgQueuePtr;
	int *arrayUsersPid;
	int msgQueueId;
	int childPid;
    int shmIndiceBloccoId;
    int *shmIndiceBloccoPtr;
    int semSetMsgQueueId;
	struct sembuf sops;
    union semun semMsgQueueUnion;      /*union semctl*/
    unsigned short semMsgQueueValArr[SO_NODES_NUM];  /*dichiaro array di union semctl*/
	int i;

    /*SOLO PER TEST*/
    #if(ENABLE_TEST)
    union semun TEST_GETALL;
    unsigned short TEST_GETALL_ARRAY[SO_NODES_NUM]; 
    #endif

	/*Inizializzazione memoria condivisa per libro mastro*/
	shmIdMastro = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);

    /*Inizializzazione memoria condivisa per indice blocco libro mastro*/
    shmIndiceBloccoId = shmget(IPC_PRIVATE, sizeof(int), 0600 | IPC_CREAT);
    /*Assegno indice iniziale all'id dei blocchi*/
    shmIndiceBloccoPtr = (int *)shmat(shmIndiceBloccoId, NULL, 0);
    *(shmIndiceBloccoPtr) = 0;

		/* SOLO PER TEST */
		#if(ENABLE_TEST)
		printf("\nid libro mastro[%d]\n", shmIdMastro);
		#endif

	/* Attach del libro mastro alla memoria condivisa*/
	transazione *libroMastro = (transazione *)shmat(shmIdMastro, NULL, 0);
	TEST_ERROR;

	/*inizializzazione memoria condivisa array ID coda di messaggi*/
	shmIdNodo = shmget(IPC_PRIVATE, SO_NODES_NUM*sizeof(int), 0600 | IPC_CREAT);
	arrayNodeMsgQueuePtr = (int *)shmat(shmIdNodo, NULL, 0);    /*eseguo attach per riempire l'array di ID di code di messaggi*/

		/* SOLO PER TEST */	
		#if(ENABLE_TEST)
		printf("\nLa shared memory dell'array contenente gli ID delle message queue è: %d\n", shmIdNodo);
		#endif

	/*riempio l'arrayCodeID con gli ID delle code messaggi prima di entrare nel for per la creazione dei nodi*/
	for(i = 0; i < SO_NODES_NUM; i++){
		msgQueueId = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
		*(arrayNodeMsgQueuePtr+i) = msgQueueId;
	}

		/* SOLO PER TEST */
		#if(ENABLE_TEST)
		for(i = 0; i < SO_NODES_NUM; i++){
			printf("STAMPO TUTTI GLI ID DELLE CODE DI MESSAGGI CREATE\n");
			printf("ID coda n %d = %d\n", i, *(arrayNodeMsgQueuePtr+i));
		}
		#endif

    /*Inizializzo set semafori per gestire invio transazioni da parte degli utenti*/
    semSetMsgQueueId = semget(IPC_PRIVATE, SO_NODES_NUM, 0600 | IPC_CREAT);
    TEST_ERROR;
    /*Popolo array valore semafori */
    for(i = 0; i < SO_NODES_NUM; i++){
        semMsgQueueValArr[i] = SO_TP_SIZE;
        printf("Valore indice %d = %d\n", i, semMsgQueueValArr[i]);
    }

    /*assegno array valori semafori alla union*/
    semMsgQueueUnion.array = semMsgQueueValArr;

    /*imposto valore semafori*/ 
    semctl(semSetMsgQueueId, 0, SETALL, semMsgQueueUnion);
    TEST_ERROR;

        /*SOLO PER TEST*/
        #if(ENABLE_TEST)
        TEST_GETALL.array = TEST_GETALL_ARRAY;
        semctl(semSetMsgQueueId, 0, GETALL, TEST_GETALL);
        for(i = 0; i < SO_NODES_NUM; i++){
            printf("Valore semaforo ID : %d -> %d\n", i, TEST_GETALL.array[i]);
        }
        exit(12345);
        #endif

	/*inizializzazione memoria condivisa array PID processi utente*/
	shmIdUtente = shmget(IPC_PRIVATE, SO_USERS_NUM*sizeof(int), 0600 | IPC_CREAT);
	arrayUsersPid = (int *)shmat(shmIdUtente, NULL, 0);

		/* SOLO PER TEST */
		#if(ENABLE_TEST)
		printf("\nLa shared memory dell'array contenente gli ID delle message queue è: %d\n", shmIdUtente);
		#endif	

	/*FOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOORK*/
	/*
  	for (i=0; i<SO_NODES_NUM; i++) {

    childPid = fork();

    switch (childPid){

    case -1:
      printf("errore, ciao\n");
      exit(EXIT_FAILURE);
      
    case 0:
		printf("PID=%d, PPID=%d, i=%d, fork_value=%d\n", getpid(), getppid(), i, childPid);
		printf("ciao, sono NODO\n");
		exit(EXIT_SUCCESS);
    default:
    printf("I'm a mother\n");
    }
  }*/

	/* creazione figli users con operazioni annesse */
	for (i=0; i<SO_USERS_NUM; i++) {
		switch (childPid = fork())
		{

		case -1:
			fprintf(stderr, "%s, %d: Errore (%d) nella fork\n",
					__FILE__, __LINE__, errno);
			exit(EXIT_FAILURE);

		case 0:
				/* SOLO PER TEST*/
				#if(ENABLE_TEST)
				printf("sono il figio: %d, in posizione: %d\n", getpid(), i);
				#endif

			exit(EXIT_SUCCESS);
			break;

		default:
            /* riempimento array PID utenti nella memoria condivisa */
			*(arrayUsersPid + i) = childPid;
			break;
		}
	}
			/* SOLO PER TEST*/
			#if(ENABLE_TEST)
			for(i = 0; i < SO_USERS_NUM; i++){
				printf("\nSTAMPO TUTTI GLI ID DEGLI UTENTI NELL'ARRAY CREATE\n");
				printf("PID utente %d = %d\n", i, *(arrayUsersPid+i));
			}
			exit(12345);
			#endif

            

	/*CHIUSURA LIBRO MASTRO*/
	shmctl(shmIdMastro, IPC_RMID, 0);
	TEST_ERROR;
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
            if(strcmp(token, "SO_USERS_NUM") == 0)
            {
                SO_USERS_NUM = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_NODES_NUM") == 0)
            {
                SO_NODES_NUM = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_REWARD") == 0)
            {
                SO_REWARD = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_MIN_TRANS_GEN_NSEC") == 0)
            {
                SO_MIN_TRANS_GEN_NSEC =  atol(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_MAX_TRANS_GEN_NSEC") == 0)
            {
                SO_MAX_TRANS_GEN_NSEC =  atol(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_RETRY") == 0)
            {
                SO_RETRY = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_TP_SIZE") == 0)
            {
                SO_TP_SIZE = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_BLOCK_SIZE") == 0)
            {
                SO_BLOCK_SIZE = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_MIN_TRANS_PROC_NSEC") == 0)
            {
                SO_MIN_TRANS_PROC_NSEC = atol(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_MAX_TRANS_PROC_NSEC") == 0)
            {
                SO_MAX_TRANS_PROC_NSEC = atol(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_REGISTRY_SIZE") == 0)
            {
                SO_REGISTRY_SIZE = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_BUDGET_INIT") == 0)
            {
                SO_BUDGET_INIT = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_SIM_SEC") == 0)
            {
                SO_SIM_SEC = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_FRIENDS_NUM") == 0)
            {
                SO_FRIENDS_NUM = atoi(strtok(NULL, delimeter));
            }
            else {
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
		#if(ENABLE_TEST)
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