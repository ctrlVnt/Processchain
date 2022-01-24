#include <time.h>
#include <stdio.h>


#define nsec_sleep 1500000000

int main()
{
	long ntos = 1e9L;
	int sec = nsec_sleep / ntos;
	long nsec = nsec_sleep - (sec * ntos);
	printf("SEC: %d\n", sec);
	printf("NSEC: %ld\n", nsec);
	struct timespec tempoDiAttesa;
	tempoDiAttesa.tv_sec = sec;
	tempoDiAttesa.tv_nsec = nsec;
	nanosleep(&tempoDiAttesa, NULL);
}