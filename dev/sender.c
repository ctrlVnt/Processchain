#define _GNU_SOURCE
#include <stdio.h>	/*input/output*/
#include <stdlib.h> /*effettuare exit()*/
#include <errno.h>	/*prelevare valore errno*/
#include <string.h> /*operazioni stringhe*/

#include <sys/shm.h>   /*memoria condivisa*/
#include <sys/ipc.h>   /*recuperare i flag delle strutture system V*/
#include <sys/types.h> /*per compatiblità*/
#include <sys/sem.h> /*operazione sui semafori*/
#include <sys/msg.h> /*operazione sulle code di messaggi*/

#include <unistd.h> /*per getpid() etc*/

#include<time.h>
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
#define SO_NODES_NUM 5
#define SO_USERS_NUM 10
#define SO_BUDGET_INIT 100
#define SO_REWARD 10
#define TRANSACTION_SIZE sizeof(transazione)

typedef struct transazione
{							   /*inviata dal processo utente a uno dei processi nodo che la gestisce*/
	struct timespec timestamp; /*tempo transazione*/
	pid_t sender;			   /*valore sender (getpid())*/
	pid_t receiver;			   /*valore receiver -> valore preso da array processi UTENTE*/
	int quantita;			   /*denaro inviato*/
	int reward;				   /*denaro inviato al nodo*/
} transazione;

/*struct per invio messaggio*/
typedef struct mymsg{
	long mtype;
	transazione transazione;
}message;

int cQuantita(int budget);

int main(){
	int shmIdNodo;
	int *arrayNodeMsgQueuePtr;
	int msgQueueId;
	int i;
	int childPid;
	int shmIdUtente;
	int *arrayUsersPid;
	/*aggiunta 19 dicembre*/
	int nodoScelto;
	struct timespec timestampTransazione;
	transazione transazioneInvio;
	int receiver;/*utente a cui invio la transazione*/
	int budgetUtente;
	int quantita;
	int reward;
	message messaggio;
	
	 
	/*inizializzazione memoria condivisa array PID processi utente*/
	shmIdUtente = shmget(IPC_PRIVATE, SO_USERS_NUM*sizeof(int), 0600 | IPC_CREAT);
	arrayUsersPid = (int *)shmat(shmIdUtente, NULL, 0);
	 
	/*test con chiave non IPC_PRIVATE array code messaggi dei nodi*/
	shmIdNodo = shmget(12345, SO_NODES_NUM*sizeof(int), 0600 | IPC_CREAT);
	arrayNodeMsgQueuePtr = (int *)shmat(shmIdNodo, NULL, 0);    /*eseguo attach per riempire l'array di ID di code di messaggi*/

		/* SOLO PER TEST */	
		#if(ENABLE_TEST)
		printf("\nLa shared memory dell'array contenente gli ID delle message queue è: %d\n", shmIdNodo);
		#endif

	/*riempio l'arrayCodeID con gli ID delle code messaggi prima di entrare nel for per la creazione dei nodi*/
	for(i = 0; i < SO_NODES_NUM; i++){
		msgQueueId = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
		*(arrayNodeMsgQueuePtr+i) = msgQueueId;
		printf("ID ciascuna coda di msg %d\n", msgQueueId);
	}
	sleep(1);
	/*ciclo creaziones*/
for (i=0; i<SO_USERS_NUM; i++) {
		switch (childPid = fork())
		{

		case -1:
			fprintf(stderr, "%s, %d: Errore (%d) nella fork\n",
					__FILE__, __LINE__, errno);
			exit(EXIT_FAILURE);

		case 0:
			/*sincronizzazione fasulla in attesa di Boris*/
			sleep(1);
			budgetUtente = SO_BUDGET_INIT;
			/*-----------------------------AGGIUNTA 19 Dicembre--------------------------------*/
			if(budgetUtente >= 2){
				/*ESEGUO ATTACH ARRAY DI CODE MSG FUORI DA EVENTUALE FUNZIONE PER INVIO TRANSAZIONE*/	
				shmIdNodo = shmget(12345, SO_NODES_NUM*sizeof(int), 0600 | IPC_CREAT);
				arrayNodeMsgQueuePtr = (int *)shmat(shmIdNodo, NULL, 0);    /*eseguo attach per accedere l'array di ID di code di messaggi*/
				/*seleziono il nodo a cui inviare -> EVENTUALE FUNZIONE PER CREAZIONE TRANSAZIONE*/
				srand(getpid());
				nodoScelto = rand()%SO_NODES_NUM;
				/*printf("Io utente %d ho scelto il nodo in pos %d\n", getpid(), nodoScelto);*/
				/*riempio transazione per l'invio*/
				transazioneInvio.sender = getpid();
					/*brutto ma funziona, ovviamente creeremo una funzione che contiene ciò*/
					while(getpid() == (receiver = *(arrayUsersPid + (rand()%SO_USERS_NUM)))){
						receiver = *(arrayUsersPid + (rand()%SO_USERS_NUM));
					}
				transazioneInvio.receiver = receiver;
					/*prendo il timestamp*/
					clock_gettime(CLOCK_BOOTTIME, &timestampTransazione);
				transazioneInvio.timestamp = timestampTransazione;
					/*printf("secondi %ld, nanosecondi %ld\n", transazioneInvio.timestamp.tv_sec, transazioneInvio.timestamp.tv_nsec);*/
					/*printf("Io utente %d ho scelto di inviare all'utente %d\n", getpid(), receiver);*/
					/*calcolo soldi da inviare*/
				quantita = cQuantita(budgetUtente);
					/*printf("Io utente %d invio in totale %d euro\n",getpid(),quantita);*/
					/*calcolo il reward*/
				reward = (quantita*SO_REWARD)/100;
				if(reward == 0){
					reward = 1;
				}
				/*calcolo soldi totali da inviare*/
				transazioneInvio.quantita = quantita - reward;
				/*	printf("Io utente %d ho scelto di inviare %d euro all'utente\n",getpid(),transazioneInvio.quantita);*/
				/*imposto reward da inviare*/
				transazioneInvio.reward = reward;
					/*printf("Io utente %d ho inviato %d euro REWARD\n",getpid(),transazioneInvio.reward);*/
				/*sottraggo al budget i soldi appena inviati*/
				budgetUtente -= quantita;
			}else{
				/*non invio transazione*/
			}
			/*PER TEST COMPOSIZIONE TRANSAZIONE*/
			/*	printf("%d TIMESTAMP:", i); printf("%ld secondi, ", transazioneInvio.timestamp.tv_sec); 
				printf("%ld nanosecondi\n", transazioneInvio.timestamp.tv_nsec);
				printf("%d SENDER: %d\n",i, transazioneInvio.sender);
				printf("%d RECEIVER: %d\n",i, transazioneInvio.receiver);
				printf("%d QUANTITA': %d\n",i, transazioneInvio.quantita);
				printf("%d REWARD: %d\n",i, transazioneInvio.reward);
				printf("%d BUDGET RIMANENTE %d\n", i, budgetUtente); */
			/*-----------------------------FINE CREAZIONE TRANSAZIONE-------------------------------*/
			/*-----------------------------CREAZIONE MESSAGGIO DA INVIARE-------------------------------*/
				messaggio.mtype = 10;
				messaggio.transazione = transazioneInvio;
				msgsnd(*(arrayNodeMsgQueuePtr+nodoScelto), &messaggio, TRANSACTION_SIZE, IPC_NOWAIT);
				TEST_ERROR;
				printf("%d ID CODA DI MESSAGGI A CUI INVIO %d\n", i, *(arrayNodeMsgQueuePtr+nodoScelto));
				
					
			/*-----------------------------FINE CREAZIONE MESSAGGIO DA INVIARE-------------------------------*/
			/*-----------------------------FINE AGGIUNTA 19 Dicembre--------------------------------*/
			exit(EXIT_SUCCESS);
			break;

		default:
            /* riempimento array PID utenti nella memoria condivisa */
			*(arrayUsersPid + i) = childPid;
			/*printf("user in pos %d è %d\n", i, *(arrayUsersPid + i));*/
			break;
		}
	}
  exit(EXIT_SUCCESS);
}
/*funzioni di supporto*/
int cQuantita(int budget){
	int res = 0;
	srand(getpid());
	while((res = rand()%budget) <= 1){
		res = rand()%budget;
	}
	return res;
}
