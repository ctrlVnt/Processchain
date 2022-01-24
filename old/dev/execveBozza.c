#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/*
NODO avra' bisogno di:
1. ID della SM che contiene il libro mastro -> INT (read+wreat)
2. ID della SM che contiene l'id indice libro mastro ->INT (read+write)
3. ID della SM che contiene il numero di NODI stessi -> INT (read)
3. ID della SM da dove recuperare l'id della MQ del nodo a cui inviare la transazione(VERSIONE NORMAL)-> INT (read)
4. ID della SM che contiene i PARAMETRI -> INT (read)
*/

int main()
{
    /*VARIABILI DI TEST*/
    int idShmLibroMastro = 1234;
    int idShmIndiceLibroMastro = 1123;
    int idShmNodesNumbers = 1113;
    int isShmParametriSimulazione = 1111;

    /*QUESTO DOVRA' ESEGUIRE PROC. PRIMA DELLA EXECVE*/
    /*pattern di esempio: char arr[MAX_NUMBER_STRINGS][MAX_STRING_SIZE]; */
    char parameters[5][32];
    // bzero(parameters, 6*32);
    char intToStrBuff[32];
    strcpy(parameters[0], "execveBozzaReceiver");
    /*trasformo INT to STRING*/
    sprintf(intToStrBuff, "%d", idShmLibroMastro); /*funzione che converte INT to STRING mediante output formatted conversion*/
    strcpy(parameters[1], intToStrBuff);
    sprintf(intToStrBuff, "%d", idShmIndiceLibroMastro);
    strcpy(parameters[2], intToStrBuff);
    sprintf(intToStrBuff, "%d", idShmNodesNumbers);
    strcpy(parameters[3], intToStrBuff);
    sprintf(intToStrBuff, "%d", isShmParametriSimulazione);
    strcpy(parameters[4], intToStrBuff);
    /*STAMPO L'ARRAY COSI' OTTENUTO*/
    for(int i = 0; i < 5; i++)
    {
        printf("%s\n", parameters[i]);
    }
    /*TEST SE EFFETTIVAMENTE RIESCO A PASSARE QUESTI ARGOMENTI*/
    switch (fork())
    {
    case -1:
        perror("fork");
        return -1;
        break;
    case 0:
        printf("execve\n");
        int r = execlp("./execveBozzaReceiver", parameters[0], parameters[1], parameters[2], parameters[3], parameters[4], NULL);
        printf("%d\n", r);
        perror("execve");
        break;
    default:
        break;
    }
}