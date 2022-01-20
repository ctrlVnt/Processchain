#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(int argc, char const *argv[])
{
	int shmId = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600);
	
	if(shmId != -1)
	{
		int *intPnt = (int *)shmat(shmId, NULL, 0);
		*intPnt = 1;
		printf("Valore contenuto nel segmento condviso: %d\n", *(intPnt+1));
		shmctl(shmId, IPC_RMID, NULL);
	}
	return 0;
}