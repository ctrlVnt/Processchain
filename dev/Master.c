#include <stdio.h>  //input/output
#include <stdlib.h> //effettuare exit()
#include <errno.h>  //prelevare valore errno
#include <string.h> //operazioni stringhe

#include <sys/shm.h>//memoria condivisa
#include <sys/ipc.h> //recuperare i flag delle strutture system V
#include <sys/types.h>//per compatiblità 

#include <unistd.h>//per getpid() etc
//indicare che è stata copiata dal codice visto a lezione...
#define TEST_ERROR    if (errno) {fprintf(stderr, \
					  "%s:%d: PID=%5d: Error %d (%s)\n", \
					  __FILE__,			\
					  __LINE__,			\
					  getpid(),			\
					  errno,			\
					  strerror(errno));}

typedef struct transazione{      //inviata dal processo utente a uno dei processi nodo che la gestisce
	struct timespec timestamp;   //tempo transazione
	pid_t sender;                //valore sender (getpid())
	pid_t receiver;				 //valore receiver -> valore preso da array processi UTENTE
	int quantita; 				 //denaro inviato
	int reward;					 //denaro inviato al nodo
} transazione;

int main(){
	int SO_REGISTRY_SIZE, SO_BLOCK_SIZE;
	SO_REGISTRY_SIZE = 100; //valore da inizializzare a runtime da file parameters.txt
	SO_BLOCK_SIZE = 10;     //valore da inizializzare a runtime da file parameters.txt
	
	
	int shm_ID = shmget(IPC_PRIVATE, SO_REGISTRY_SIZE * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);
	
	printf("Dimensione %ld struct\n", sizeof(transazione));
	transazione *libroMastro = (transazione *)shmat(shm_ID, NULL, 0);
	TEST_ERROR;
	
	//libroMastro[999* sizeof(transazione)].sender = 10;
	//printf("pid: %d\n",libroMastro[999*sizeof(transazione)].sender);
	int i = 0;
	/*while(1){
		printf("Dimensione %ld libroMastro[%d]\n", sizeof(libroMastro[i]), i);
		i++;
	}*/
	
	printf("Dimensione %ld libroMastro[15000]\n", sizeof(libroMastro[15000]));
	
	shmctl(shm_ID, IPC_RMID, 0);
	TEST_ERROR;
}






















