#include "process_chain.h"
/*Proposta lista argomenti ricevuto dalla EXECLP*/
/*
I parametri della simulazione(comuni a tutti i nodi):
0.Nome file eseguibile
1.SO_MIN_TRANS_PROC_NSEC
2.2 SO_MAX_TRANS_PROC_NSEC
3.ID SM dove salvo gli ID delle code di messaggi -- Magari introdurre un campo HEADER a indicare il numero di code presenti
4.1 Il mio numero d'ordine(quando si vuole recupeprare il valore che effettivamente appartiene a me - faccio numeroORDINE + 1)
5.ID SM dove si trova libro mastro
6.ID SM dove si trova l'indice del libro mastro
7.SO_HOPS
8.ID semaforo dell'indice libro mastro
9.ID semaforo Mia coda di messaggi
10. SO_TP_SIZE1
11. SO_BLOCK_SIZE1
12. SO_REGISTRY_SIZE
13. SO_FRINEDS_NUM
14. idSharedMemoryAmiciNodi
*/
/****************************************/
/*STRUTTURE*/

/*inviata dal processo utente a uno dei processi nodo che la gestisce*/
typedef struct transazione_ds
{
    /*tempo transazione*/
    struct timespec timestamp;
    /*valore sender (getpid())*/
    pid_t sender;
    /*valore receiver -> valore preso da array processi UTENTE*/
    pid_t receiver;
    /*denaro inviato*/
    int quantita;
    /*denaro inviato al nodo*/
    int reward;
} transazione;

/*struct per invio messaggio*/
typedef struct mymsg
{
    long mtype;
    transazione transazione;
    int hops;
} message;

/*struttura che tiene traccia dello stato dell'utente*/
typedef struct nodo_ds
{
    /*id della sua coda di messaggi*/
    int mqId;
    /*Proposta di RICCARDO*/
    int budget;
    /*getpid()*/
    int nodoPid;
    int transazioniPendenti;
    //int *friends;
} nodo;
/***********/

/*VARIABILI GLOBALI*/
/*La capacita' della transaction pool associata a ciascun nodo per immagazzinare le transazioni da elaborare - NOTA A COMPILE TIME*/
int SO_TP_SIZE1;
/*Le transazioni sono elaborate in blocchi, la variabile seguente determina la sua capacita', che dev'essere minore rispetto alla capacita' della transacion pool - NOTA A COMPILE TIME*/
int SO_BLOCK_SIZE1;
int SO_REGISTRY_SIZE;
int SO_FRIENDS_NUM;
/**/
long soMinTransProcNsec;
/**/
long soMaxTransProcNsec;
/*ID della SM degli'ID delle CODE di messaggi alla quale devo fare l'attach*/
int idSharedMemoryTuttiNodi;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente gli id di tutte le code di messaggi, attenzione primo campo e' HEADER*/
nodo *puntatoreSharedMemoryTuttiNodi;
/*
Il mio numero d'ordine
Si assume che alla cella numeroOrdine-esima nella SM puntato da 
 *puntatoreSharedMemoryTuttiNodi posso effettivamente recuperare l'ID della mia MQ.
*/
int numeroOrdine;
/*Id della mia coda di messaggi*/
int idMiaMiaCodaMessaggi;
/*ID della SM dove si trova il libro mastro*/
int idSharedMemoryLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria dove si trova effettivamente il libro mastro*/
transazione *puntatoreSharedMemoryLibroMastro;
/*ID SM dove si trova l'indice del libro mastro*/
int idSharedMemoryIndiceLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria condivisa dove si trova effettivamente l'indice del libro mastro*/
int *puntatoreSharedMemoryIndiceLibroMastro;
/*id Shared memory dove ci sono gli amici del nodo*/
int idSharedMemoryAmiciNodi;
/*Dopo l'attach punta alla porzione di memoria condivisa dove ci sono gli amici del nodo*/
int *puntatoreSharedMemoryAmiciNodi;

/*
Id del semaforo che regola l'input sul libro mastro
Il semaforo di tipo binario.
*/
int SO_HOPS;
/*
Id del semaforo che regola l'accesso all'indice
Il semaforo di tipo binario.
*/
int idSemaforoAccessoIndiceLibroMastro;
/*Id del semaforo che regola l'accesso alla MQ associata al nodo*/
int idSemaforoAccessoMiaCodaMessaggi;
/*variabili ausiliari*/
/*variabile usata come secondo parametro nella funzione strtol, come puntatore dove la conversionbe della stringa ha avuto fine*/
char *endptr;
/*
quasta variabile contiene il numero di numero totale dei nodi attivi nella simulazione
REMINDER: per recuperare il valore, bisogna accedere alla 0-sime cella della shared memory contenente tutti gli id delle MQ associate 1-1 ad ogni nodo.
*/
int numeroTotaleNodi;
/*variabile struttura per effettuare le operazioni sul semaforo*/
struct sembuf operazioniSemaforo;
/*variabile determina ciclo di vita del nodo*/
int node;
/*variabili d'appoggio*/
int shmdtRisposta;
int tpPiena;
int indiceSuccessivoTransactionPool;
int indiceSuccessivoBlocco;
message messaggioRicevuto;
int msgrcvRisposta;
int semopRisposta;
transazione *transactionPool;
transazione *blocco;
transazione transazioneReward;
int indiceLibroMastroRiservato;
int riempimentoTpStop;
int bloccoRiempibile;
/*indice ausiliario*/
int i;

/*variabili strutture per gestire segnali*/

struct sigaction sigactionSigusr1Nuova;
struct sigaction sigactionSigusr1Precedente;
sigset_t maskSetForSigusr1;

/*funzioni relative al nodo*/
void parseThenStoreParameter(int *dest, char const *argv[], int num, int flag);
/*gesrtione del segnale*/
void sigusr1Handler(int sigNum);
void attesaNonAttiva(long nsecMin, long nsecMax);
void aggiornaBilancioNodo(int indiceLibroMastroRiservato);

int main(int argc, char const *argv[])
{
    printf("%s: nodo[%d] e ha ricevuto #%d parametri.\n", argv[0], getpid(), argc);

    /*INIZIALIZZO VARIABILI A COMPILE TIME*/
    SO_TP_SIZE1 = 10;
    SO_BLOCK_SIZE1 = 7;

    /*LIMITI DI TEMPO*/
    /*Recupero primo parametro*/
    soMinTransProcNsec = strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[1], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (soMinTransProcNsec == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    else if ((soMinTransProcNsec == LONG_MIN || soMinTransProcNsec == LONG_MAX) && errno == ERANGE)
    {
        perror("Errore ERANGE");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    // printf("soMinTransProcNsec: %ld\n", soMinTransProcNsec);

    /*Recupero secondo parametro*/
    soMaxTransProcNsec = strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[2], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (soMaxTransProcNsec == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    else if ((soMaxTransProcNsec == LONG_MIN || soMaxTransProcNsec == LONG_MAX) && errno == ERANGE)
    {
        perror("Errore d");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    // printf("soMaxTransProcNsec: %ld\n", soMaxTransProcNsec);

    /******************/

    /*ID DELLA SH DI TUTTE LE CODE DI MESSAGGI*/
    idSharedMemoryTuttiNodi = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[3], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (idSharedMemoryTuttiNodi == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    // printf("idSharedMemoryTuttiNodi: %d\n", idSharedMemoryTuttiNodi);

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryTuttiNodi = (nodo *)shmat(idSharedMemoryTuttiNodi, NULL, 0);
    if (errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }
    /*Recupero il mio numero d'ordine*/
    numeroOrdine = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[4], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (numeroOrdine == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    // printf("numeroOrdine: %d\n", numeroOrdine);
    // printf("ID mia coda di messaggi - %d\n", puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].mqId);

    idSharedMemoryLibroMastro = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[5], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (idSharedMemoryLibroMastro == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    // printf("idSharedMemoryLibroMastro: %d\n", idSharedMemoryLibroMastro);

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryLibroMastro = (transazione *)shmat(idSharedMemoryLibroMastro, NULL, 0);
    if (errno == 22)
    {
        perror("shmat puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }

    idSharedMemoryIndiceLibroMastro = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[6], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (idSharedMemoryIndiceLibroMastro == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    // printf("idSharedMemoryIndiceLibroMastro: %d\n", idSharedMemoryIndiceLibroMastro);

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryIndiceLibroMastro = (int *)shmat(idSharedMemoryIndiceLibroMastro, NULL, 0);
    if (*puntatoreSharedMemoryIndiceLibroMastro == -1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    SO_HOPS = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[7], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (SO_HOPS == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    // printf("soHops: %d\n", SO_HOPS);

    idSemaforoAccessoIndiceLibroMastro = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[8], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (idSemaforoAccessoIndiceLibroMastro == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    // printf("idSemaforoAccessoIndiceLibroMastro: %d\n", idSemaforoAccessoIndiceLibroMastro);

    idSemaforoAccessoMiaCodaMessaggi = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[9], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (idSemaforoAccessoMiaCodaMessaggi == 0 && errno == EINVAL)
    {
        perror("Errore di conversione");
        exit(EXIT_FAILURE);
    }
    /*TEST*/
    // printf("idSemaforoAccessoMiaCodaMessaggi: %d\n", idSemaforoAccessoMiaCodaMessaggi);
    SO_TP_SIZE1 = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[10], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (SO_TP_SIZE1 == 0 && errno == EINVAL)
    {
        perror("Errore di conversione SO_TP_SIZE1");
        exit(EXIT_FAILURE);
    }
    #if(ENABLE_TEST)
        printf(" N SO_TP_SIZE1: %d\n", SO_TP_SIZE1);
    #endif
    SO_BLOCK_SIZE1 = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[11], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (SO_BLOCK_SIZE1 == 0 && errno == EINVAL)
    {
        perror("Errore di conversione SO_BLOCK_SIZE1");
        exit(EXIT_FAILURE);
    }
    #if(ENABLE_TEST)
        printf("N SO_BLOCK_SIZE1: %d\n", SO_BLOCK_SIZE1);
    #endif
    SO_REGISTRY_SIZE = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[12], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (SO_REGISTRY_SIZE == 0 && errno == EINVAL)
    {
        perror("Errore di conversione SO_REGISTRY_SIZE");
        exit(EXIT_FAILURE);
    }
    #if(ENABLE_TEST)
        printf("N SO_REGISTRY_SIZE: %d\n", SO_REGISTRY_SIZE);
    #endif
    SO_FRIENDS_NUM= (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[13], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (SO_FRIENDS_NUM == 0 && errno == EINVAL)
    {
        perror("Errore di conversione SO_FRIENDS_NUM");
        exit(EXIT_FAILURE);
    }
    #if(ENABLE_TEST)
        printf("N SO_FRIENDS_NUM: %d\n", SO_FRIENDS_NUM);
    #endif
    idSharedMemoryAmiciNodi = (int)strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[14], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
    if (idSharedMemoryAmiciNodi == 0 && errno == EINVAL)
    {
        perror("Errore di conversione idSharedMemoryAmiciNodi");
        exit(EXIT_FAILURE);
    }
    #if(ENABLE_TEST)
        printf("N idSharedMemoryAmiciNodi: %d\n", idSharedMemoryAmiciNodi);
    #endif
    puntatoreSharedMemoryAmiciNodi = (int *)shmat(idSharedMemoryAmiciNodi, NULL, 0);
    if (*puntatoreSharedMemoryAmiciNodi == -1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    /*imposto l'handler*/
    sigactionSigusr1Nuova.sa_flags = 0;
    sigemptyset(&sigactionSigusr1Nuova.sa_mask);
    sigactionSigusr1Nuova.sa_handler = sigusr1Handler;
    sigaction(SIGUSR1, &sigactionSigusr1Nuova, &sigactionSigusr1Precedente);

    /*POSSO INIZIARE A GESTIRE LE TRANSAZIONI*/

    node = NODO_CONTINUE;
    tpPiena = 0;
    /*alloco dinamicamente la TP e blocco*/
    transactionPool = (transazione *)calloc(SO_TP_SIZE1, sizeof(transazione));
    if (errno == EINVAL)
    {
        perror("calloco transaction pool");
        exit(EXIT_FAILURE);
    }
    blocco = (transazione *)calloc(SO_BLOCK_SIZE1, sizeof(transazione));
    if (errno == EINVAL)
    {
        perror("calloco transaction pool");
        exit(EXIT_FAILURE);
    }
    /*setto alcuni campi della transazine di REWARD*/
    transazioneReward.quantita = 0;
    transazioneReward.receiver = getpid();
    transazioneReward.sender = SENDER_NODO;
    transazioneReward.reward = 0;
    clock_gettime(CLOCK_REALTIME, &transazioneReward.timestamp);
    indiceSuccessivoTransactionPool = 0;
    indiceSuccessivoBlocco = 0;
    puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].transazioniPendenti = 0;

    riempimentoTpStop = 1;
    bloccoRiempibile = 0;

    while (node)
    {
        while (tpPiena < SO_TP_SIZE1 && node == NODO_CONTINUE && riempimentoTpStop)
        {
            msgrcvRisposta = msgrcv(puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].mqId, &messaggioRicevuto, sizeof(messaggioRicevuto.transazione) + sizeof(messaggioRicevuto.hops), 0, IPC_NOWAIT);

            /*USARE VARIABILE PER INVIO TRANSAZIONE A NODI AMICI
             if(invioAdAmico == 1){
                msgsend();
                invioAdAmico == 0;
             }*/

            // printf("[%d] ha effettuato la msgrcv dalla coda ID[%d] con risposta: %d\n", getpid(), puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].mqId, msgrcvRisposta);
            if(errno != EAGAIN && msgrcvRisposta != -1){
                tpPiena++;
                bloccoRiempibile++;
                transactionPool[(indiceSuccessivoTransactionPool++)] = messaggioRicevuto.transazione;
                puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].transazioniPendenti++;

                if(indiceSuccessivoTransactionPool == SO_TP_SIZE1)
                {
                    indiceSuccessivoTransactionPool = 0;
                }
            }else{
                errno = 0;
                if(bloccoRiempibile == SO_BLOCK_SIZE1 - 1){
                    riempimentoTpStop = 0;
                }else{
                    attesaNonAttiva(soMinTransProcNsec, soMaxTransProcNsec);
                }
            }
            
            
        }
        if(node == NODO_STOP)
        {
            break; /*esco dal ciclo esterno*/
        }
        /*TEST*/
        // printf("[%d] Mia TP e' piena\n", getpid());
        for (i = 0; i < (SO_BLOCK_SIZE1 - 1); i++)
        {
            blocco[i] = transactionPool[(indiceSuccessivoBlocco++)];
            transazioneReward.quantita += blocco[i].reward;
            if (indiceSuccessivoBlocco == SO_TP_SIZE1)
            {
                indiceSuccessivoBlocco = 0;
            }
        }
        blocco[SO_BLOCK_SIZE1 - 1] = transazioneReward;
        transazioneReward.quantita = 0;
        /*TEST*/
        // printf("[%d] Ho costruito il blocco: quanita' di reward e' %d\n", getpid(), transazioneReward.quantita);
        /*devo riservare l'indice del libro mastro*/
        operazioniSemaforo.sem_flg = 0;
        operazioniSemaforo.sem_num = 0;
        operazioniSemaforo.sem_op = -1;
        semopRisposta = semop(idSemaforoAccessoIndiceLibroMastro, &operazioniSemaforo, 1);
        /*possibile codice superfluo perchè la semop non è mai bloccante*/
        /*if (semopRisposta == -1)
        {
            perror("Tentativo di riservare l'indice del libro mastro fallito\n");
            node = NODO_STOP;
            break;
        }*/
        indiceLibroMastroRiservato = *(puntatoreSharedMemoryIndiceLibroMastro);
        if (indiceLibroMastroRiservato >= SO_REGISTRY_SIZE) /*11 parametro settato*/
        {
            node = NODO_STOP;
            break;
        }
        *(puntatoreSharedMemoryIndiceLibroMastro) += 1;
        /*release dell'indice libro mastro*/
        operazioniSemaforo.sem_flg = 0;
        operazioniSemaforo.sem_num = 0;
        operazioniSemaforo.sem_op = 1;
        semopRisposta = semop(idSemaforoAccessoIndiceLibroMastro, &operazioniSemaforo, 1);
        /*possibile codice superfluo perchè la semop non è mai bloccante*/
        /*if (semopRisposta == -1)
        {
            perror("Tentativo di rilasciare l'indice del libro mastro fallito\n");
            node = NODO_STOP;
        }*/
        attesaNonAttiva(soMinTransProcNsec, soMaxTransProcNsec);
        /*ho riservato l'indice - posso scrivere sul libro mastro*/
        for (i = 0; i < SO_BLOCK_SIZE1; i++)
        {
            puntatoreSharedMemoryLibroMastro[indiceLibroMastroRiservato * SO_BLOCK_SIZE1 + i] = blocco[i];
        }
        /*TEST*/
        // printf("[%d] ho finito di scrivere sul libro mastro\n", getpid());
        /*rilascio il semaforo*/
        operazioniSemaforo.sem_flg = 0;
        operazioniSemaforo.sem_num = numeroOrdine;
        operazioniSemaforo.sem_op = SO_BLOCK_SIZE1 - 1;
        semopRisposta = semop(idSemaforoAccessoMiaCodaMessaggi, &operazioniSemaforo, 1);
        /*if (semopRisposta == -1)
        {
            perror("Tentativo di rilasciare semaforo mia coda di messaggi\n");
            node = NODO_STOP;
        }*/
        /**/
        tpPiena -= SO_BLOCK_SIZE1 - 1;
        bloccoRiempibile = tpPiena;
        riempimentoTpStop = 1;
        puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].transazioniPendenti -=  SO_BLOCK_SIZE1 - 1;
        /*TEST*/
        // printf("Nuovo ciclo\n");
        /*aggiorno bilancio del nodo*/
        aggiornaBilancioNodo(indiceLibroMastroRiservato);

        /*USARE VARIABILE PER INVIO TRANSAZIONE A NODI AMICI, RIMETTO A ZERO
          invioAdAmico = 1;*/
    }

    /*****************************************/

    /*CICLO DI VITA DEL NODO TERMINA*/
    /*Deallocazione delle risorse*/
    shmdtRisposta = shmdt(puntatoreSharedMemoryAmiciNodi);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryAmiciNodi");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryIndiceLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiNodi);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }
    printf("Risorse deallocate correttamente\n");
    return EXIT_SUCCESS;
}

/*
La seguente funzione, in base a NUM, recupera il valore dall'array argv e lo salva nella variabile puntata da dest
*/
void parseThenStoreParameter(int *dest, char const *argv[], int num, int flag)
{
    if (flag == IS_LONG || flag == IS_INT)
    {
        *dest = strtol(/*PRIMO PARAMETRO DELLA LISTA EXECVE*/ argv[num], /*PUNTATTORE DI FINE*/ &endptr, /*BASE*/ 10);
        if (*dest == 0 && errno == EINVAL)
        {
            perror("Errore di conversione");
            exit(EXIT_FAILURE);
        }
        else if (flag == IS_LONG && (*dest == LONG_MIN || *dest == LONG_MAX) && errno == ERANGE)
        {
            perror("Errore ERANGE");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        fprintf(stderr, "Flag %d non e' definito in questa funzione!\n", flag);
        exit(EXIT_FAILURE);
    }

    return;
}

void sigusr1Handler(int sigNum)
{
    // printf("NODO SIGUSR1 ricevuto\n");
    node = NODO_STOP;
}

void attesaNonAttiva(long nsecMin, long nsecMax)
{
    srand(getpid());
    long ntos = 1e9L;
    long diff = nsecMax - nsecMin;
    long attesa = rand() % diff + nsecMin;
    int sec = attesa / ntos;
    long nsec = attesa - (sec * ntos);
    /*TEST*/
    //printf("SEC: %d\n", sec);
    //printf("NSEC: %ld\n", nsec);
    struct timespec tempoDiAttesa;
    tempoDiAttesa.tv_sec = sec;
    tempoDiAttesa.tv_nsec = nsec;
    nanosleep(&tempoDiAttesa, NULL);
    /*TODO - MASCERARE IL SEGNALE SIGCONT*/
}

/*visto che lavoriamo con le SM, provare a delegare questa operazione al libro mastro*/
void aggiornaBilancioNodo(int indiceLibroMastroRiservato)
{
    int ultimoIndice;
    ultimoIndice = *(puntatoreSharedMemoryIndiceLibroMastro);
    
    for(indiceLibroMastroRiservato; indiceLibroMastroRiservato < ultimoIndice; indiceLibroMastroRiservato++)
    {
        if(puntatoreSharedMemoryLibroMastro[indiceLibroMastroRiservato*SO_BLOCK_SIZE1 + (SO_BLOCK_SIZE1 - 1)].receiver == getpid())
        {
            puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].budget += puntatoreSharedMemoryLibroMastro[indiceLibroMastroRiservato*SO_BLOCK_SIZE1 + (SO_BLOCK_SIZE1 - 1)].quantita;
        }
    }
    /*TEST*/
    // printf("[%d] aggiornamento bilancio terminato\n", getpid());
}
