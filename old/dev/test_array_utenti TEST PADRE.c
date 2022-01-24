/*FUNZIONA -> TEST EFFETTUATI : 5 */

#include <stdio.h>	
#include <stdlib.h> 
#include <errno.h>	
#include <string.h>

#include <sys/shm.h>  
#include <sys/ipc.h>  
#include <sys/types.h>
#include <sys/msg.h>    /*include necessario per le code di messaggi*/
#include <sys/sem.h>    /*operazioni su semafori*/

#include <unistd.h> 

#define SO_USERS_NUM 20

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

int main(){

	pid_t my_pid;
	/*pid_t my_ppid;*/
	pid_t value;
	int shm_ID;
	int sem_ID;
	struct sembuf sops;   /*struttura per efettuare operazioni su semaforo*/
	int *arrayUtenti;
	int i;
	
	shm_ID = shmget(IPC_PRIVATE, SO_USERS_NUM*sizeof(int), 0600 | IPC_CREAT);
	arrayUtenti = (int *)shmat(shm_ID, NULL, 0);
	printf("La shared memory dell'array contenente gli ID delle message queue è: %d\n", shm_ID);
	
	/*creo un semaforo per la gestione dell'accesso da parte degli utenti all'array*/
	sem_ID = semget(IPC_PRIVATE, 1, 0600);  /*il flag IPC_CREAT penso si possa omettere*/
	TEST_ERROR;
	semctl(sem_ID, 0, SETVAL, 1);           /*inizializzo un semaforo binario*/
	TEST_ERROR;

	/*imposto ID e flag che sono sempre questi visto il mutex. Il flag IPC_NOWAIT nel nostro caso non serve
	   visto che vogliamo che tutti possano scrivere il proprio PID nell'array*/
	sops.sem_num = 0;
	sops.sem_flg = 0;
	
	/* DENTRO I FIGLI */
	for (i=0; i<SO_USERS_NUM; i++) {
	
		switch (value = fork()) {
		case -1:
			/* Handle error */
			fprintf(stderr, "%s, %d: Errore (%d) nella fork\n",
				__FILE__, __LINE__, errno);
			exit(EXIT_FAILURE);
			
		case 0:
			printf("Sono il figlio %d e sto scrivendo nell'array generato in pos: %d\n", getpid(),i);
			exit(EXIT_SUCCESS);
		break;
		default:
		
			*(arrayUtenti+i) = value;
		break;
		}
	}
	sleep(2);
	/*test avvenuta scrittura su array degli utenti da parte dei processi utente*/
	for(i = 0; i < SO_USERS_NUM; i++){
		printf("STAMPO TUTTI GLI ID DELLE CODE DI MESSAGGI CREATE\n");
		printf("ID coda n %d = %d\n", i, *(arrayUtenti+i));
	}
	exit(EXIT_SUCCESS);
}





