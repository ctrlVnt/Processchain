#include <stdio.h>	//input/output
#include <stdlib.h> //effettuare exit()
#include <errno.h>	//prelevare valore errno
#include <string.h> //operazioni stringhe

#include <sys/shm.h>   //memoria condivisa
#include <sys/ipc.h>   //recuperare i flag delle strutture system V
#include <sys/types.h> //per compatiblità

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

typedef struct transazione
{							   //inviata dal processo utente a uno dei processi nodo che la gestisce
	struct timespec timestamp; //tempo transazione
	pid_t sender;			   //valore sender (getpid())
	pid_t receiver;			   //valore receiver -> valore preso da array processi UTENTE
	int quantita;			   //denaro inviato
	int reward;				   //denaro inviato al nodo
} transazione;

int main()
{
	int SO_REGISTRY_SIZE, SO_BLOCK_SIZE;
	SO_REGISTRY_SIZE = 10; //valore da inizializzare a runtime da file parameters.txt
	SO_BLOCK_SIZE = 10;		//valore da inizializzare a runtime da file parameters.txt
	
	
	int SO_NODES_NUM, SO_USERS_NUM;
	SO_USERS_NUM = 3;
	SO_NODES_NUM = 5;
	int indice;
  	unsigned int my_pid, my_ppid, value;

	// creo n figli nodi

  	for (indice=0; indice<SO_NODES_NUM; indice++) {

    value = fork();

    switch (value){
    case -1:
      printf("errore, ciao\n");
      exit(EXIT_FAILURE);
      
    case 0:
		printf("PID=%d, PPID=%d, i=%d, fork_value=%d\n", getpid(), getppid(), indice, value);
		printf("ciao, sono NODO\n");
		exit(EXIT_SUCCESS);
    default:
    printf("I'm a mother\n");
    }
  }
	// creo n figli user
  for (indice=0; indice<SO_USERS_NUM; indice++) {

    value = fork();

    switch (value)
    {
    case -1:
      printf("errore, ciao\n");
      exit(EXIT_FAILURE);
      
    case 0:
      printf("PID=%d, PPID=%d, i=%d, fork_value=%d\n", getpid(), getppid(), indice, value);
      printf("ciao, sono USER\n");
		exit(EXIT_SUCCESS);
    default:
    printf("I'm a mother\n");
    }
  }

	int shm_ID = shmget(4321, SO_REGISTRY_SIZE * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);
	printf("id[%d]\n", shm_ID);

	printf("Dimensione %ld struct\n", sizeof(transazione));
	transazione *libroMastro = (transazione *)shmat(shm_ID, NULL, 0);
	TEST_ERROR;

	for(int i = 0; i < SO_REGISTRY_SIZE; i++)
	{
		for (int j = 0; j < SO_BLOCK_SIZE; j++)
		{
			libroMastro[i*SO_BLOCK_SIZE + j].quantita = rand()%10+1;
			libroMastro[i*SO_BLOCK_SIZE + j].reward = rand()%10+1;
		}
		
	}
	printf("Finito\n");

	sleep(10);

	shmctl(shm_ID, IPC_RMID, 0);
	TEST_ERROR;
}
