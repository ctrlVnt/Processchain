#include "process_chain.h"

/*Proposta lista argomenti ricevuto dalla EXECLP*/
/*
I parametri della simulazione(comuni a tutti gli utenti):
0.Nome file eseguibile
1.SO_MIN_TRANS_GEN_NSEC
2.2 SO_MAX_TRANS_GEN_NSEC
3.ID SM dove salvo gli ID delle code di messaggi -- Magari introdurre un campo HEADER a indicare il numero di code presenti
4.Il mio numero d'ordine
5.ID SM dove si trova libro mastro
6.SO_RETRY
7.SO_BUDGET_INIT
8.ID SM dove ci sono tutti gli pid degli utenti - destinatari
9.ID semaforo per accedere alla coda di mex
10. ID semaforo accesso libro mastro
11. SO_REWARD
12. SO_TP_SIZE
13. SO_BLOCK_SIZE
*/
/****************************************/
/***********/

/*VARIABILI GLOBALI*/
int soFriendsNum;
int soHops;
/**/
int soRetry;
/**/
int soBudgetInit;
/**/
/*long soMinTransGenNsec;*/
/**/
/*long soMaxTransGenNsec;*/
/*ID della SM degli'ID delle CODE di messaggi alla quale devo fare l'attach*/
int idSharedMemoryTuttiNodi;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente gli id di tutte le code di messaggi, attenzione primo campo e' HEADER*/
nodo *puntatoreSharedMemoryTuttiNodi;
/*Id del semaforo che regola l'accesso alla MQ associata al nodo*/
int idSemaforoAccessoCodeMessaggi;
/*
Il mio numero d'ordine
*/
int numeroOrdine;
/*ID della SM dove si trova il libro mastro*/
int idSharedMemoryLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria dove si trova effettivamente il libro mastro*/
transazione *puntatoreSharedMemoryLibroMastro;
/*ID SM dove si trova l'indice del libro mastro*/
int idSharedMemoryIndiceLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria condivisa dove si trova effettivamente l'indice del libro mastro*/
int *puntatoreSharedMemoryIndiceLibroMastro;
/*ID della SM contenente tutti i PID dei processi utenti - visti come destinatari*/
int idSharedMemoryTuttiUtenti;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente i PID degli utenti*/
utente *puntatoreSharedMemoryTuttiUtenti;
/*id Shared memory dove ci sono gli amici del nodo*/
int idSharedMemoryAmiciNodi;
/*Dopo l'attach punta alla porzione di memoria condivisa dove ci sono gli amici del nodo*/
int *puntatoreSharedMemoryAmiciNodi;
/*numero totale di utenti*/
int numeroTotaleUtenti;
/*variabile struttura per effettuare le operazioni sul semaforo*/
struct sembuf operazioniSemaforo;
/*variabile determina ciclo di vita dell'utente*/
int user;

/*******************/

/*variabili strutture per gestire i segnali*/

/*sigaction da settare per gestire ALARM - timer della simulazione*/
struct sigaction sigactionSigusr1Nuova;
/*sigaction dove memorizzare lo stato precedente*/
struct sigaction sigactionSigusr1Precedente;
/*maschera per SIGALARM*/
sigset_t maskSetForSigusr1;
/*sigaction generica - uso generale*/
struct sigaction act;
/*old sigaction generica - uso generale*/
struct sigaction actPrecedente;

/*******************************************/

/*variabli ausiliari*/
/*variabile usata come secondo parametro nella funzione strtol, come puntatore dove la conversionbe della stringa ha avuto fine*/
char *endptr;
int shmdtRisposta;
int msgsndRisposta;
transazione transazioneInvio;
message messaggio;
int indiceLibroMastroRiservato;
struct timespec timespecRand;
/*int SO_REWARD;*/
/********************/

/*FUNZIONI*/

void sigusr1Handler(int sigNum);
int scegliUtenteRandom(int max);
int scegliNumeroCoda(int max);
int getQuantitaRandom(int max);
int getBudgetUtente(int indiceLibroMastroRiservato);
void attesaNonAttiva(long nsecMin, long nsecMax);
/**********/

int main(int argc, char const *argv[])
{
    /*variabili locali*/
    int q;
    int idCoda;
    int semopRisposta;

#if (ENABLE_TEST)
    printf("\t%s: utente[%d] e ha ricevuto #%d parametri.\n", argv[0], getpid(), argc);
#endif
    indiceLibroMastroRiservato = 0;
    setSoMinTransGenNsec(parseLongFromParameters(argv, 1));
/*TEST*/
#if (ENABLE_TEST)
    printf("\tsoMinTransProcNsec: %ld\n", getSoMinTransGenNsec());
#endif
    setSoMaxTransGenNsec(parseLongFromParameters(argv, 2));
/*TEST*/
#if (ENABLE_TEST)
    printf("\tsoMaxTransProcNsec: %ld\n", getSoMaxTransGenNsec());
#endif
    /*ID DELLA SH DI TUTTE LE CODE DI MESSAGGI*/
    idSharedMemoryTuttiNodi = parseIntFromParameters(argv, 3);
/*TEST*/
#if (ENABLE_TEST)
    printf("\tidSharedMemoryTutteCodeMessaggi: %d\n", idSharedMemoryTuttiNodi);
#endif
    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryTuttiNodi = (nodo *)shmat(idSharedMemoryTuttiNodi, NULL, 0);
    if (errno == EINVAL)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    /*Recupero il mio numero d'ordine*/
    numeroOrdine = parseIntFromParameters(argv, 4);
/*TEST*/
#if (ENABLE_TEST)
    printf("\tnumeroOrdineUtente: %d\n", numeroOrdine);
#endif
    /*idSharedMemoryLibroMastro = (int)strtol(argv[5], &endptr, 10);*/
    idSharedMemoryLibroMastro = parseIntFromParameters(argv, 5);
/*TEST*/
#if (ENABLE_TEST)
    printf("\tidSharedMemoryLibroMastro: %d\n", idSharedMemoryLibroMastro);
#endif
    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryLibroMastro = (transazione *)shmat(idSharedMemoryLibroMastro, NULL, 0);
    if (errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    /*Recupero SO_RETRY*/
    setSoRetry(parseIntFromParameters(argv, 6));
/*TEST*/
#if (ENABLE_TEST)
    printf("\tsoRetry: %d\n", getSoRetry());
#endif
    /*Recupero SO_BUDGET_INIT*/
    setSoBudgetInit(parseIntFromParameters(argv, 7));
/*TEST*/
#if (ENABLE_TEST)
    printf("\tsoBudgetInit: %d\n", getSoBudgetInit());
#endif
    idSharedMemoryTuttiUtenti = parseIntFromParameters(argv, 8);
/*TEST*/
#if (ENABLE_TEST)
    printf("\tidSharedMemoryTuttiUtenti: %d\n", idSharedMemoryTuttiUtenti);
#endif

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryTuttiUtenti = (utente *)shmat(idSharedMemoryTuttiUtenti, NULL, 0);
    if (errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }
    numeroTotaleUtenti = puntatoreSharedMemoryTuttiUtenti[0].userPid;
#if (ENABLE_TEST)
    printf("\tNumero totale di Utenti: %d\n", puntatoreSharedMemoryTuttiUtenti[0].userPid);
#endif
    idSemaforoAccessoCodeMessaggi = parseIntFromParameters(argv, 9);
/*TEST*/
#if (ENABLE_TEST)
    printf("\tidSemaforoAccessoCodeMessaggi: %d\n", idSemaforoAccessoCodeMessaggi);
#endif

    /*Mi atacco alla SM che contiene l'indice del libro mastro*/
    idSharedMemoryIndiceLibroMastro = parseIntFromParameters(argv, 10);
#if (ENABLE_TEST)
    printf("\tidSharedMemoryIndiceLibroMastro: %d\n", idSharedMemoryIndiceLibroMastro);
#endif
    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryIndiceLibroMastro = (int *)shmat(idSharedMemoryIndiceLibroMastro, NULL, 0);
#if (ENABLE_TEST)
    printf("\tValore indiceLibroMastro: %d\n", *puntatoreSharedMemoryIndiceLibroMastro);
#endif
    setSoReward(parseIntFromParameters(argv, 11));
#if (ENABLE_TEST)
    printf("\t U SO_REWARD: %d\n", getSoReward());
#endif
#if (ENABLE_TEST)
    printf("\t U SO_TP_SIZE: %d\n", getTpSize());
#endif
#if (ENABLE_TEST)
    printf("U SO_BLOCK_SIZE: %d\n", getSoBlockSize());
#endif
    setSoFriendsNum(parseIntFromParameters(argv, 14));
#if (ENABLE_TEST)
    printf("U soFriendsNum: %d\n", getSoFriendsNum());
#endif
    setSoHops(parseIntFromParameters(argv, 15));
#if (ENABLE_TEST)
    printf("U soHops: %d\n", getSoHops());
#endif
    idSharedMemoryAmiciNodi = parseIntFromParameters(argv, 16);
#if (ENABLE_TEST)
    printf("U idSharedMemoryAmiciNodi: %d\n", idSharedMemoryAmiciNodi);
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

    /*ciclo di vita utente*/
    user = UTENTE_CONTINUE;
    transazioneInvio.sender = getpid();
#if (ENABLE_TEST)
    printf("\t[%d] inizia\n", getpid());
#endif
    while (user == UTENTE_CONTINUE)
    {
#if (ENABLE_TEST)
        printf("Sono utente %d-iesimo: %d - %d\n", (numeroOrdine + 1), getpid(), puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].userPid);
#endif
        indiceLibroMastroRiservato = getBudgetUtente(indiceLibroMastroRiservato);
        transazioneInvio.receiver = scegliUtenteRandom(numeroTotaleUtenti);
        if (transazioneInvio.receiver == -1)
        {
            break;
        }
        transazioneInvio.reward = 0;
        idCoda = scegliNumeroCoda(puntatoreSharedMemoryTuttiNodi[0].nodoPid);
        if (puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget >= 2)
        {
            /*tentativo riservare un posto nella coda*/
            /*operazioniSemaforo.sem_flg = IPC_NOWAIT;
            operazioniSemaforo.sem_num = idCoda - 1;
            operazioniSemaforo.sem_op = -1;
            semopRisposta = semop(idSemaforoAccessoCodeMessaggi, &operazioniSemaforo, 1);*/
            semopRisposta = semReserve(idSemaforoAccessoCodeMessaggi, -1, (idCoda - 1), IPC_NOWAIT);
            if (semopRisposta == -1 && errno == EAGAIN)
            {
                /*caso coda TP del nodo scelto occupata*/
                /*ESTRAIAMO UN AMICO DEL NODO CHE HA IL SEMAFORO PIENO CERCHIAIMO DI INVIARE A LUI E RIPETIAMO*/
                messaggio.hops--;
                attesaNonAttiva(getSoMinTransGenNsec(), getSoMaxTransGenNsec());
            }
            else
            {
#if (ENABLE_TEST)
                printf("[%d] ho abbastanza budget %d\n", getpid(), puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget);
#endif
                q = getQuantitaRandom(puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget);
                transazioneInvio.reward = (q * getSoReward()) / 100;
                if (transazioneInvio.reward == 0)
                {
                    transazioneInvio.reward = 1;
                }
                transazioneInvio.quantita = q - transazioneInvio.reward;

                clock_gettime(CLOCK_REALTIME, &transazioneInvio.timestamp);
                messaggio.mtype = getpid();
                messaggio.transazione = transazioneInvio;
                messaggio.hops = getSoHops();
                msgsndRisposta = msgsnd(puntatoreSharedMemoryTuttiNodi[idCoda].mqId, &messaggio, sizeof(messaggio.transazione) + sizeof(messaggio.hops), 0);
                if (errno == EAGAIN && msgsndRisposta == -1)
                {
#if (ENABLE_TEST)
                    printf("Coda scelta occupata...\n");
                    * /
#endif
                        setSoRetry(getSoRetry() - 1);
                }
                else if (msgsndRisposta != -1)
                {
                    //printf("[%d] messaggio inviato con risposta %d\n", getpid(), msgsndRisposta);
                    puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget -= transazioneInvio.quantita;
                    attesaNonAttiva(getSoMinTransGenNsec(), getSoMaxTransGenNsec());
                }
            }
        }
        else
        {
            // printf("\nFACCIO SO RETRY\n");
            setSoRetry(getSoRetry() - 1);
            attesaNonAttiva(getSoMinTransGenNsec(), getSoMaxTransGenNsec());
        }
        if (getSoRetry() <= 0)
        {
#if (ENABLE_TEST)
            printf("SO_RETRY: %d\n", getSoRetry());
#endif
            puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].stato = USER_KO;
            user = UTENTE_STOP;
        }
    }

    /*"Notifico il master della mia morte"*/

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
    shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiUtenti);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryTuttiUtenti");
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
        perror("shmdt puntatoreSharedMemoryTutteCodeMessaggi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("Risorse deallocate correttamente\n");
#endif
    if (user == UTENTE_STOP)
    {
        exit(EXIT_PREMAT);
    }
    else
    {
        exit(EXIT_SUCCESS);
    }
}

void sigusr1Handler(int sigNum)
{
    // printf("UTENTE SIGUSR1 ricevuto\n");
    user = UTENTE_STOP;
    signal(SIGUSR1, SIG_DFL);
}

/*funzione che restituisce il pid dell'utente random*/
int scegliUtenteRandom(int max)
{
    int pidRandom;
    int iRandom;
    clock_gettime(CLOCK_REALTIME, &timespecRand);
    srand(timespecRand.tv_nsec);
    do
    {
        /*if(puntatoreSharedMemoryTuttiUtenti[0].userPid != 1){
		    iRandom = rand()%max + 1;
		
		    if(puntatoreSharedMemoryTuttiUtenti[iRandom].stato == USER_KO)
		    {
		        pidRandom = puntatoreSharedMemoryTuttiUtenti[iRandom].userPid;
		    }
		}else{
			return -1;
		}
        printf("%d\n", pidRandom);*/
        if (puntatoreSharedMemoryTuttiUtenti[0].userPid > 1)
        {
            iRandom = rand() % max + 1; /*cosi' scelgo tra 1 e SO_USER_NUM*/
            if (puntatoreSharedMemoryTuttiUtenti[iRandom].userPid == getpid() || puntatoreSharedMemoryTuttiUtenti[iRandom].stato == USER_KO)
            {
#if (ENABLE_TEST)
                printf("[%d]PID identico oppure utente non raggiungibile: [%d]\n", getpid(), puntatoreSharedMemoryTuttiUtenti[iRandom].userPid);
#endif
            }
            else
            {
#if (ENABLE_TEST)
                printf("Utente [%d]-esimo con PID [%d] ha scelto %d [%d]-iesimo\n", (numeroOrdine + 1), getpid(), pidRandom, iRandom);
#endif
                return pidRandom = puntatoreSharedMemoryTuttiUtenti[iRandom].userPid;
            }
        }
    } while (/*pidRandom == getpid()*/ 1);
    return pidRandom;
}

int scegliNumeroCoda(int max)
{
#if (ENABLE_TEST)
    printf("Scelgo coda...\n");
#endif
    int iCoda;
    clock_gettime(CLOCK_REALTIME, &timespecRand);
    srand(timespecRand.tv_nsec);
    iCoda = rand() % max + 1;
#if (ENABLE_TEST)
    printf("Ho scelto coda%d\n", iCoda);
#endif
    return iCoda;
}

int getQuantitaRandom(int max)
{
    clock_gettime(CLOCK_REALTIME, &timespecRand);
    srand(timespecRand.tv_nsec);
    return rand() % (max - 2 + 1) + 2;
}

int getBudgetUtente(int indiceLibroMastroRiservato)
{
    int ultimoIndice;
    int c;
    ultimoIndice = *puntatoreSharedMemoryIndiceLibroMastro;

    for (indiceLibroMastroRiservato; indiceLibroMastroRiservato < ultimoIndice; indiceLibroMastroRiservato++)
    {
        for (c = 0; c < (getSoBlockSize() - 1); c++)
        {
            if (puntatoreSharedMemoryLibroMastro[indiceLibroMastroRiservato * getSoBlockSize() + c].receiver == getpid())
            {
                puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget += puntatoreSharedMemoryLibroMastro[indiceLibroMastroRiservato * getSoBlockSize() + c].quantita;
            }
        }
    }
/*TEST*/
#if (ENABLE_TEST)
    printf("[%d] aggiornamento bilancio terminato\n", getpid());
#endif
    return indiceLibroMastroRiservato;
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
#if (ENABLE_TEST)
    printf("SEC: %d\n", sec);
    printf("NSEC: %ld\n", nsec);
#endif
    struct timespec tempoDiAttesa;
    tempoDiAttesa.tv_sec = sec;
    tempoDiAttesa.tv_nsec = nsec;
    nanosleep(&tempoDiAttesa, NULL);
#if (ENABLE_TEST)
    printf("FINITO DI ATTENDERE!!!!");
#endif
}
