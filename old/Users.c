#include "MacroChain.h"

int main(int argc, char *argv[]){
    pid_t users; //questa è la fork per i figli per le transazioni
    int status; //stato di ogni processo quando sarà terminato

    for (int i=0; i<NUM_FORKS; i++) {

    users = fork();
 
    switch (users){
    case -1:
        perror("Fork fallita dal processo USER!\n");
        exit(EXIT_FAILURE);
      
    case 0:
      printf("PID=%d, PPID=%d, i=%d, fork_value=%d\n", getpid(), getppid(), i, users); //DEBUG
      exit (EXIT_SUCCESS);

    default:
    printf("I'm a mother\n");
    }
  }
  exit(EXIT_SUCCESS);
}