#include <time.h>

int main(){
struct timespec tempoDiAttesa;
    clock_gettime(&tempoDiAttesa, NULL);

/*

    clock_gettime( CLOCK_REALTIME, &end ); 

float tempoTrascorso = (end.tv_sec - start.tv_sec) + (1e-9)*(end.tv_nsec - start.tv_nsec);
printf("%ld\n", tempoTrascorso);*/
/*printf("%ld\n", tempoDiAttesa.tv_nsec);*/
int ciao = (int)tempoDiAttesa.tv_nsec;
printf("%d\n", ciao);
}