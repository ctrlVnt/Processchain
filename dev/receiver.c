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

typedef struct transazione
{							   /*inviata dal processo utente a uno dei processi nodo che la gestisce*/
	struct timespec timestamp; /*tempo transazione*/
	pid_t sender;			   /*valore sender (getpid())*/
	pid_t receiver;			   /*valore receiver -> valore preso da array processi UTENTE*/
	int quantita;			   /*denaro inviato*/
	int reward;				   /*denaro inviato al nodo*/
} transazione;

int main(){

	int shmIdNodo;
	int *arrayNodeMsgQueuePtr;
	int msgQueueId;
	int i;
	int childPid;
	
	shmIdNodo = shmget(12345, SO_NODES_NUM*sizeof(int), 0600 | IPC_CREAT);
	arrayNodeMsgQueuePtr = (int *)shmat(shmIdNodo, NULL, 0);    /*eseguo attach per riempire l'array di ID di code di messaggi*/
	printf("Id array di code di messaggi %d\n",shmIdNodo );
	
	
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
  }
}
