#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
    Questo mini-programma esemplifica la parsificazione dei parametri utilizzati nelle simulazioni del progetto di SO
    questo repo rappresenta proprio il progetto.
    
    L'idea di base: parsificare i parametri da un file TXT chiamato "parameters.txt",
    la scelta dell'estensione del file e' arbitraria, il deve-team ha scelto come estensione .TXT, appunto.
    
    I parametri presenti nel file secondo il seguente pattern: NOME_VARIABILE=VALORE_ASSEGNATO.
    Carattere '=' in questo caso e' chiamato delimitatore.

    Breve panoramica dell'algoritmo:
    - File viene aperto in lettura
    - Viene parsificato per riga, ovvero sequenza il cui carattere finale e' '\n',
    per questo motivo abbiamo utilizzato la funzione fgets (man fgets per approfondimenti) che accumula nel buffer i caratteri finche' non raggiunge CR('\n') oppure EOF
    -Ogni blocco cosi' letto viene tokennizato -- funzione strtok che prende due paramteri: stringa da espezionare e il delimitatore, che non e' altro che una stringa
    indicante quali byte di caratteri sono consedirati delimitatori, quindi restituisce il puntatore a stringa che contenuta tra caratteri-delimitatori, quando non ci sono piu' token utili, viene restituito NULL
    (man strtok per approfondimenti)
    -Considerando il pattern della stringa otteuta da fgets, possiamo desumenre che ad ogni lettura, successivamente vengono generati 2 token(in condizioni ottimali):
    primo rappresenta il nome della variabile a cui vogliamo assegnare il valore che segue, la discriminazione delle variabile e' effettuata tramite la funzione strcmp
    che ritorna 0 se le 2 stringhe passate come argomento sono lessicograficamente identiche. (man strcmp per approfondimenti)
    -Token successivo che viene ricevuto non e' altro che il valore, che a sua volta viene castato con la funzione atoX, in base al tipom di valore, che puo' essere
    un intero oppure un intero long -- attenzione atoX non gestiscono eventuali errori!
    -La lettura viene effettuata fino al raggiungimento della fine del FILE(EOF).
    -Quindi i parametri cosi' ottenuti vengono stampati a video.
    
    P.S.: La parsificazione cosi' defenita prevede un minimo di controllo sul nome della variabile, nel caso si verifica un errore di parsing, il file viene
    chiuso e il programma termina.
    P.S.S: Possibili miglioramenti: assumere che nessun parametro possa avere il valore 0(nel caso di atoX fallito, viene restituito lo 0), quindi
    effetuare il controllo in ogni blocco dove appare la chiamata della funzione atoX e segnalare la presenza dell'eventuale errore.
*/

int SO_USERS_NUM;
int SO_NODES_NUM;
int SO_REWARD;
long SO_MIN_TRANS_GEN_NSEC;
long SO_MAX_TRANS_GEN_NSEC;
int SO_RETRY;
int SO_TP_SIZE;
int SO_BLOCK_SIZE;
long SO_MIN_TRANS_PROC_NSEC;
long SO_MAX_TRANS_PROC_NSEC;
int SO_REGISTRY_SIZE;
int SO_BUDGET_INIT;
int SO_SIM_SEC;
int SO_FRIENDS_NUM;

int main()
{
    char buffer[256] = {};
    char *token;
    char *delimeter = "= ";
    int parsedValue;
    bzero(buffer, 256);
    FILE *configFile = fopen("parameters.txt", "r+");
    if (configFile != NULL)
    {
        // printf("File aperto!\n");
        while (fgets(buffer, 256, configFile) > 0)
        {
            token = strtok(buffer, delimeter);
            if(strcmp(token, "SO_USERS_NUM") == 0)
            {
                SO_USERS_NUM = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_NODES_NUM") == 0)
            {
                SO_NODES_NUM = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_REWARD") == 0)
            {
                SO_REWARD = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_MIN_TRANS_GEN_NSEC") == 0)
            {
                SO_MIN_TRANS_GEN_NSEC =  atol(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_MAX_TRANS_GEN_NSEC") == 0)
            {
                SO_MAX_TRANS_GEN_NSEC =  atol(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_RETRY") == 0)
            {
                SO_RETRY = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_TP_SIZE") == 0)
            {
                SO_TP_SIZE = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_BLOCK_SIZE") == 0)
            {
                SO_BLOCK_SIZE = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_MIN_TRANS_PROC_NSEC") == 0)
            {
                SO_MIN_TRANS_PROC_NSEC = atol(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_MAX_TRANS_PROC_NSEC") == 0)
            {
                SO_MAX_TRANS_PROC_NSEC = atol(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_REGISTRY_SIZE") == 0)
            {
                SO_REGISTRY_SIZE = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_BUDGET_INIT") == 0)
            {
                SO_BUDGET_INIT = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_SIM_SEC") == 0)
            {
                SO_SIM_SEC = atoi(strtok(NULL, delimeter));
            }
            else if(strcmp(token, "SO_FRIENDS_NUM") == 0)
            {
                SO_FRIENDS_NUM = atoi(strtok(NULL, delimeter));
            }
            else {
                printf("Errore durante il parsing dei parametri - token %s\n", token);
                fclose(configFile);
                exit(EXIT_FAILURE);
            }

            // while(token != NULL)
            // {
            //     printf("%s", token);
            //     token = strtok(NULL, "=");
            // }
        }
        // printf("\n");
        int closeResponse = fclose(configFile);
        if (closeResponse != -1)
        {
            // printf("\n___________________________\n\nFile chiuso correttamente!\n");
        }
        else
        {
            perror("fclose");
            exit(EXIT_FAILURE);
        }

        printf("Stampo i parametri parsati!\n");
        printf("SO_USERS_NUM=%d\n", SO_USERS_NUM);
        printf("SO_NODES_NUM=%d\n", SO_NODES_NUM);
        printf("SO_REWARD=%d\n", SO_REWARD);
        printf("SO_MIN_TRANS_GEN_NSEC=%ld\n", SO_MIN_TRANS_GEN_NSEC);
        printf("SO_MAX_TRANS_GEN_NSEC=%ld\n", SO_MAX_TRANS_GEN_NSEC);
        printf("SO_RETRY=%d\n", SO_RETRY);
        printf("SO_TP_SIZE=%d\n", SO_TP_SIZE);
        printf("SO_BLOCK_SIZE=%d\n", SO_BLOCK_SIZE);
        printf("SO_MIN_TRANS_PROC_NSEC=%ld\n", SO_MIN_TRANS_PROC_NSEC);
        printf("SO_MAX_TRANS_PROC_NSEC=%ld\n", SO_MAX_TRANS_PROC_NSEC);
        printf("SO_REGISTRY_SIZE=%d\n", SO_REGISTRY_SIZE);
        printf("SO_BUDGET_INIT=%d\n", SO_BUDGET_INIT);
        printf("SO_SIM_SEC=%d\n", SO_SIM_SEC);
        printf("SO_FRIENDS_NUM=%d\n", SO_FRIENDS_NUM);
    }
    else
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
}
