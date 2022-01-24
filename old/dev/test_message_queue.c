/*FUNZIONA -> TEST EFFETTUATI: non ricordo+2*/

#include <stdio.h>	
#include <stdlib.h> 
#include <errno.h>	
#include <string.h>

#include <sys/shm.h>  
#include <sys/ipc.h>  
#include <sys/types.h>
#include <sys/msg.h>    /*include necessario per le code di messaggi*/

#include <unistd.h> 

#define SO_NODES_NUM 20

int main(){
	int i = 0;
	int msg_ID, shm_ID;
	int *arrayCodeID;	
		
	/*creo memoria condivisa per array di ID di code di messaggi -> MASTER*/
	shm_ID = shmget(IPC_PRIVATE, SO_NODES_NUM*sizeof(int), 0600 | IPC_CREAT);
	arrayCodeID = (int *)shmat(shm_ID, NULL, 0);    /*eseguo attach nel master per riempire l'array di ID di code di messaggi*/
	printf("La shared memory dell'array contenente gli ID delle message queue è: %d\n", shm_ID);
	
	/*riempio l'arrayCodeID con gli ID delle code messaggi prima di entrare nel for per la creazione dei nodi*/
	for(i; i < SO_NODES_NUM; i++){
		msg_ID = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
		*(arrayCodeID+i) = msg_ID;
	}
	
	/*solo per test*/
	for(i = 0; i < SO_NODES_NUM; i++){
		printf("STAMPO TUTTI GLI ID DELLE CODE DI MESSAGGI CREATE\n");
		printf("ID coda n %d = %d\n", i, *(arrayCodeID+i));
	}
	
	/*eventuale detach della memoria condivisa*/
	/*shmctl(shm_ID, IPC_RMID, 0);*/
	printf("FINE TEST\n");
	exit(EXIT_SUCCESS);
}

/*int main(){
	int a[SO_NODES_NUM]; /*sarà da allocare in memoria condivisa 
	int i = 0;
	int msg_ID;
	 
	
	for(i; i < SO_NODES_NUM; i++){
		msg_ID = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
		a[i] = msg_ID;
	}
	
	for(i = 0; i < SO_NODES_NUM; i++){
		printf("STAMPO TUTTI GLI ID DELLE CODE DI MESSAGGI CREATE\n");
		printf("ID coda n %d = %d\n", i, a[i]);
	}
	
	printf("FINE TEST\n");
}*/
