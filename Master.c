#include "MacroChain.h"

//PROCESSO MASTER - gestore di tutto il progetto, l'anello principale

int main (void) {
	pid_t master; //questo è la fork del master
	int status; //stato del master
    
    master = fork();
    switch(master){

    case -1:
      perror("Fork fallita dal processo MASTER!\n");
      exit(EXIT_FAILURE);
      
    case 0:

        printf("CHILD: %d - figlio del MASTER: %d\n", getpid(), getppid()); //DEBUG

        execve("./Users",NULL,NULL); //creare processi per le transazioni

        //non dovrebbe MAI arrivare qui
        fprintf(stderr, "%s: %d. Error #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));

        exit(EXIT_FAILURE);
      break;

    default:
      printf("MASTER: %d\n", getpid()); //DEBUG

      pid_t childPid;
      int status=0;

      if ((childPid = waitpid(-1, &status, 0))== -1) { //ultimazione dell'intero programma
        if (errno == ECHILD) {
          printf("Il MASTER [%d] ha ultimato\n", getpid());
          exit(EXIT_SUCCESS);
        } else {
          fprintf(stderr, "Error #%d: %s\n", errno, strerror(errno));
          exit(EXIT_FAILURE);
        }
      }

      if (WIFEXITED(status)) {
        printf("Stato di uscita: %d\n",WEXITSTATUS(status));
      } else {
        printf("Stato inaspettato\n");
      }

      exit(EXIT_SUCCESS);

      break;
    }
}