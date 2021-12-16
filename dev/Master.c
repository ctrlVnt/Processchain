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

<<<<<<< HEAD


int main(){
	int SO_REGISTRY_SIZE, SO_BLOCK_SIZE;
	SO_REGISTRY_SIZE = 10; //valore da inizializzare a runtime da file parameters.txt
	SO_BLOCK_SIZE = 10;     //valore da inizializzare a runtime da file parameters.txt
	
	
	//int shm_ID = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);
	int shm_ID = shmget(4321,SO_REGISTRY_SIZE * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);
	printf("ID: %d\n", shm_ID);
	//printf("Dimensione %ld struct\n", sizeof(transazione[SO_BLOCK_SIZE])*SO_REGISTRY_SIZE);
	transazione *(libroMastro) = shmat(shm_ID, NULL, 0);
=======
int main()
{
	int SO_REGISTRY_SIZE, SO_BLOCK_SIZE;
	SO_REGISTRY_SIZE = 10; //valore da inizializzare a runtime da file parameters.txt
	SO_BLOCK_SIZE = 10;		//valore da inizializzare a runtime da file parameters.txt

	int shm_ID = shmget(4321, SO_REGISTRY_SIZE * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);
	printf("id[%d]\n", shm_ID);

	printf("Dimensione %ld struct\n", sizeof(transazione));
	transazione *libroMastro = (transazione *)shmat(shm_ID, NULL, 0);
>>>>>>> 1c90f53aad003836b9a1550ebbe4da94f8ec095e
	TEST_ERROR;
<<<<<<< HEAD
=======
	
<<<<<<< HEAD
	//libroMastro[0].sender = 10;
	//printf("pid: %d\n",libroMastro[999*sizeof(transazione)].sender);
=======
	/*libroMastro[1024].sender = 10;
	printf("pid: %d\n",libroMastro[1024].sender);*/
>>>>>>> 1c90f53aad003836b9a1550ebbe4da94f8ec095e
	int i = 0;
	/*while(1){
		printf("Dimensione %ld libroMastro[%d]\n", sizeof(libroMastro[i]), i);
		i++;
	}*/
<<<<<<< HEAD
	while(i<400){
		printf("Dimensione %d libroMastro[%d]\n",libroMastro[i].sender, i);
		i++;
	}
	
	
=======
	int n;
	printf("Dimensione %ld libroMastro[n]\n", sizeof(libroMastro[n]));

	/*while(1){
		libroMastro[i].sender = i+3;
		printf("cella [%d]: %d\n",i, libroMastro[i].sender);
		i++;
	}*/
	
	//printf("Dimensione %ld libroMastro[15000]\n", sizeof(libroMastro[15000]));
>>>>>>> 1c90f53aad003836b9a1550ebbe4da94f8ec095e
	
	shmctl(shm_ID, IPC_RMID, 0);
	TEST_ERROR;
}



















>>>>>>> 2801c4464451ee981539923254364aa01f3e0665

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
