#include "process_chain.h"
#include <stdarg.h>

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
11. SO_BLOCK_SIZE
12. SO FRIENDS NUM
13. SO HOPS
14. ID SM AMICI NODI
15. idCodaMessaggiProcessoMaster
*/
/****************************************/
/***********/

/*VARIABILI GLOBALI*/
/*ID della SM degli'ID delle CODE di messaggi alla quale devo fare l'attach*/
int idSharedMemoryTuttiNodi;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente gli id di tutte le code di messaggi, attenzione primo campo e' HEADER*/
nodo *puntatoreSharedMemoryTuttiNodi;
/*Id del semaforo che regola l'accesso alla MQ associata al nodo*/
int idSemaforoAccessoCodeMessaggi;
/*Id della coda di messaggi del processo master*/
int idCodaMessaggiProcessoMaster;
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
int alrm;
int limiteSistema;
/*******************/

/*variabili strutture per gestire i segnali*/
/*sigaction da settare per gestire ALARM - timer della simulazione*/
struct sigaction sigactionSigusr1Nuova;
/*sigaction dove memorizzare lo stato precedente*/
struct sigaction sigactionSigusr1Precedente;
/*maschera per SIGALARM*/
sigset_t maskSetForSigusr1;
/*specularmente per SIGUSR2*/
struct sigaction sigactionSigusr2Nuova;
struct sigaction sigactionSigusr2Precedente;
sigset_t maskSetForSigusr2;
/*sigaction generica - uso generale*/
struct sigaction act;
/*old sigaction generica - uso generale*/
struct sigaction actPrecedente;
int idSemaforoLimiteRisorse;

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
/********************/

/*FUNZIONI*/

void sigusr1Handler(int sigNum);
void sigusr2Handler(int sigNum);
int impostaHandlerSaNoMask(struct sigaction *saNew, struct sigaction *saOld, int sigNum, void (*handler)(int));
int impostaHandlerSaWithMask(struct sigaction *saNew, sigset_t *maskSet, struct sigaction *saOld, int sigNum, void (*handler)(int), int numMasks, ...);
int scegliUtenteRandom(int max);
int scegliNumeroNodo(int max);
int getQuantitaRandom(int max);
void getBudgetUtente(int *indiceLibroMastroRiservato);
void attesaNonAttiva(long nsecMin, long nsecMax);
int inviaTransazione(int flag);
int eseguiDetachSm(void *ptr);
void deallocaRisorse();
void avvisaPoiCambiaStato();
int scegliAmicoNodo();
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
    setSoFriendsNum(parseIntFromParameters(argv, 12));
#if (ENABLE_TEST)
    printf("U soFriendsNum: %d\n", getSoFriendsNum());
#endif
    setSoHops(parseIntFromParameters(argv, 13));
#if (ENABLE_TEST)
    printf("U soHops: %d\n", getSoHops());
#endif
    idSharedMemoryAmiciNodi = parseIntFromParameters(argv, 14);
#if (ENABLE_TEST)
    printf("U idSharedMemoryAmiciNodi: %d\n", idSharedMemoryAmiciNodi);
#endif
    puntatoreSharedMemoryAmiciNodi = (int *)shmat(idSharedMemoryAmiciNodi, NULL, 0);
    if (*puntatoreSharedMemoryAmiciNodi == -1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    idCodaMessaggiProcessoMaster = parseIntFromParameters(argv, 15);
#if (ENABLE_TEST)
    printf("N idCodaMessaggiProcessoMaster: %d\n", idCodaMessaggiProcessoMaster);
#endif
    idSemaforoLimiteRisorse = parseIntFromParameters(argv, 16);
#if (ENABLE_TEST)
    printf("N idCodaMessaggiProcessoMaster: %d\n", idCodaMessaggiProcessoMaster);
#endif

    /*imposto l'handler*/
    impostaHandlerSaWithMask(&sigactionSigusr1Nuova, &maskSetForSigusr1, &sigactionSigusr1Precedente, SIGUSR1, sigusr1Handler, 1, SIGUSR2);

    /*imposto l'handler*/
    impostaHandlerSaNoMask(&sigactionSigusr2Nuova, &sigactionSigusr2Precedente, SIGUSR2, sigusr2Handler);

    /*ciclo di vita utente*/
    user = UTENTE_CONTINUE;
    alrm = ALARM_CONTINUE;
    transazioneInvio.sender = getpid();
    limiteSistema = 1;
#if (ENABLE_TEST)
    printf("\t[%d] inizia\n", getpid());
#endif

    /*ridurre ancora il ciclo di vita*/
    while (user == UTENTE_CONTINUE && alrm == ALARM_CONTINUE)
    {
#if (ENABLE_TEST)
        printf("Sono utente %d-iesimo: %d - %d\n", (numeroOrdine + 1), getpid(), puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].userPid);
#endif
        switch (inviaTransazione(0))
        {
        case UTENTE_CONTINUE:
#if (ENABLE_TEST)
            printf("SO_RETRY: %d\n", getSoRetry());
#endif
            if (getSoRetry() <= 0)
            {
                avvisaPoiCambiaStato();
            }
            break;
        case UTENTE_STOP:
        default:
            avvisaPoiCambiaStato();
            break;
        }
    }

    /*Deallocazione delle risorse*/
    deallocaRisorse();
#if (ENABLE_TEST)
    printf("Risorse deallocate correttamente\n");
#endif
    if (user == UTENTE_STOP)
    {
        exit(EXIT_FAILURE);
    }
    else
    {
        exit(EXIT_SUCCESS);
    }
}

void sigusr1Handler(int sigNum)
{
#if (ENABLE_TEST)
    printf("UTENTE SIGUSR1 ricevuto\n");
#endif
    alrm = ALLARME_SCATTATO;
    // deallocaRisorse();
    signal(SIGUSR1, SIG_DFL);
}

void sigusr2Handler(int sigNum)
{
    printf("\n\nTransazione extraordinaria!\n\n");
    inviaTransazione(1);
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
    } while (alrm != ALLARME_SCATTATO);
    return pidRandom;
}

int scegliNumeroNodo(int max)
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

void getBudgetUtente(int *indiceLibroMastroRiservato)
{
    int ultimoIndice;
    int c;
    ultimoIndice = *puntatoreSharedMemoryIndiceLibroMastro;

    for (*indiceLibroMastroRiservato; *indiceLibroMastroRiservato < ultimoIndice; (*indiceLibroMastroRiservato)++)
    {
        for (c = 0; c < (getSoBlockSize() - 1); c++)
        {
            if (puntatoreSharedMemoryLibroMastro[(*indiceLibroMastroRiservato) * getSoBlockSize() + c].receiver == getpid())
            {
                puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget += puntatoreSharedMemoryLibroMastro[(*indiceLibroMastroRiservato) * getSoBlockSize() + c].quantita;
            }
        }
    }
/*TEST*/
#if (ENABLE_TEST)
    printf("[%d] aggiornamento bilancio terminato\n", getpid());
#endif
    //return indiceLibroMastroRiservato;
}

void attesaNonAttiva(long nsecMin, long nsecMax)
{
    srand(getpid());
    long ntos = 1e9L;
    long diff = nsecMax - nsecMin;
    long attesa;
    if (diff == 0)
    {
        attesa = nsecMax;
    }
    else
    {
        attesa = rand() % diff + nsecMin;
    }
    int sec = attesa / ntos;
    long nsec = attesa - (sec * ntos);
    /*TEST*/
#if(ENABLE_TEST)
    printf("SEC: %d\n", sec);
    printf("NSEC: %ld\n", nsec);
#endif
    struct timespec tempoDiAttesa;
    tempoDiAttesa.tv_sec = sec;
    tempoDiAttesa.tv_nsec = nsec;
    nanosleep(&tempoDiAttesa, NULL);
    /*TODO - MASCERARE IL SEGNALE SIGCONT*/
}

int inviaTransazione(int flag)
{
    /*transazione che potenzialmente verra' spedita*/
    transazione t;
    message m;
    /*id del NODO, in base a questo recuperiamo tutti i dati necessari*/
    int idNodo;
    int idAmico;
    /*quantita' da inviare*/
    int q;
    int risposta;
    t.sender = getpid();
    getBudgetUtente(&indiceLibroMastroRiservato);
    t.receiver = scegliUtenteRandom(numeroTotaleUtenti);
    t.reward = 0;
    /*imposto alcuni campi che so apriori*/
    m.mtype = getpid();
    m.hops = getSoHops();
    #if(ENABLE_TEST)
        printf("IMPOSTO SO HOPS %d\n", getSoHops());
    #endif
    if (t.receiver != -1)
    {
        if (puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget >= 2)
        {
            /*scelgo un nodo a caso*/
            idNodo = scegliNumeroNodo(puntatoreSharedMemoryTuttiNodi[0].nodoPid);

            /*preparo la transazione*/
            q = getQuantitaRandom(puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget);
            t.reward = (q * getSoReward()) / 100;
            if (t.reward == 0)
            {
                t.reward = 1;
            }
            t.quantita = q - t.reward;
            clock_gettime(CLOCK_REALTIME, &t.timestamp);
            m.transazione = t;

            /*verifico se il nodo scelto e' pronto a ricevere la mia t*/
            if (semReserve(idSemaforoAccessoCodeMessaggi, -1, (idNodo - 1), IPC_NOWAIT) == -1 && errno == EAGAIN)
            {
                do
                {
                    idNodo = scegliAmicoNodo(idNodo);
                    if (semReserve(idSemaforoAccessoCodeMessaggi, -1, (idNodo - 1), IPC_NOWAIT) == -1 && errno == EAGAIN)
                    {
                        m.hops--;
                        attesaNonAttiva(getSoMinTransGenNsec(), getSoMaxTransGenNsec());
                    }
                    else
                    {
                        risposta = msgsnd(puntatoreSharedMemoryTuttiNodi[idNodo].mqId, &m, sizeof(m.transazione) + sizeof(m.hops), 0);
                        if (risposta != -1)
                        {
                            puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget -= (t.quantita + t.reward);
                        }
                        break; /*esco dal ciclo*/
                    }

                    #if(ENABLE_TEST)
                        printf("Continuo il ciclo...\n");
                    #endif
                } while (m.hops > 0 && alrm != ALLARME_SCATTATO);

                if (m.hops == 0 && limiteSistema == 1)
                {
                    /*m.hops == 0*/
                    /*devo delegare la transazione al master*/
                    errno = 0;
                    risposta = semReserve(idSemaforoLimiteRisorse, -1, 0, IPC_NOWAIT);
                    if (risposta != -1 && errno != EAGAIN)
                    {
                        risposta = msgsnd(idCodaMessaggiProcessoMaster, &m, sizeof(transazione) + sizeof(int), 0);
                        if (risposta != -1)
                        {
                            puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget -= (t.quantita + t.reward);
#if (ENABLE_TEST)
                            printf("Transazione delegata al master!\n");
#endif
                        }
                    }
                    else
                    {
                        limiteSistema = 0;
#if (ENABLE_TEST)
                        printf("LIMITE RAGGIUNTO, ERRORE\n");
#endif
                    }
                }
                else
                {
                    // printf("LIMITE RAGGIUNTO, ERRORE else esterno riga 498 UTENTE.c\n");
                    // setSoRetry(getSoRetry() - 1);
                    #if(ENABLE_TEST)
                    printf("SO_RETRY: %d\n", getSoRetry());
                    #endif
                }

                /*attendo*/
                attesaNonAttiva(2 * getSoMinTransGenNsec(), 2 * getSoMaxTransGenNsec());
            }
            /*So che c'e' almeno una cella libera nella TP*/
            else
            {
                /*invio la transazione*/
                if (flag)
                {
#if (ENABLE_TEST)
                    printf("\n\n\nTRANSAZIONE EXTRA: %d - %d con q=%d\n\n\n", t.sender, t.receiver, t.quantita);
#endif
                }
                risposta = msgsnd(puntatoreSharedMemoryTuttiNodi[idNodo].mqId, &m, sizeof(m.transazione) + sizeof(m.hops), 0);
                if (risposta == -1 && errno == EAGAIN)
                {
#if (ENABLE_TEST)
                    printf("Coda scelta occupata...\n");
#endif
                    setSoRetry(getSoRetry() - 1);
                }
                else if (risposta != -1)
                {
                    /*aggiorno budget*/
                    puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget -= (t.quantita + t.reward);
                    /*attesa non attiva*/
                    attesaNonAttiva(getSoMinTransGenNsec(), getSoMaxTransGenNsec());
                }
            }
        }
        else
        {
            /*caso budget insufficiente - decremento soRetry*/
            setSoRetry(getSoRetry() - 1);
            if (getSoRetry() <= 0)
            {
                return UTENTE_STOP;
            }
        }

        return UTENTE_CONTINUE;
    }
    else
    {
        /*caso no utenti disponibili*/
        return UTENTE_STOP;
    }
}

int impostaHandlerSaNoMask(struct sigaction *saNew, struct sigaction *saOld, int sigNum, void (*handler)(int))
{
    saNew->sa_flags = 0;
    saNew->sa_handler = handler;
    sigemptyset(&saNew->sa_mask);
    return sigaction(sigNum, saNew, saOld);
}

int impostaHandlerSaWithMask(struct sigaction *saNew, sigset_t *maskSet, struct sigaction *saOld, int sigNum, void (*handler)(int), int numMasks, ...)
{
    int masksCount;
    va_list valist;
    saNew->sa_flags = 0;
    saNew->sa_handler = handler;
    sigemptyset(maskSet);

    /*UTILIZZO VARARGS PER IMPOSTARE LE MASCHERE*/
    va_start(valist, numMasks);
    for (masksCount = 0; masksCount < numMasks; masksCount++)
    {
        sigaddset(maskSet, va_arg(valist, int));
    }
    va_end(valist);

    saNew->sa_mask = *maskSet;
    return sigaction(sigNum, saNew, saOld);
}

void deallocaRisorse()
{
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryAmiciNodi);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryAmiciNodi");
        exit(EXIT_FAILURE);
    }*/
    eseguiDetachShm(puntatoreSharedMemoryAmiciNodi, "puntatoreSharedMemoryAmiciNodi U");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryIndiceLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }*/
    eseguiDetachShm(puntatoreSharedMemoryIndiceLibroMastro, "puntatoreSharedMemoryIndiceLibroMastro U");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiUtenti);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }*/
    eseguiDetachShm(puntatoreSharedMemoryTuttiUtenti, "puntatoreSharedMemoryTuttiUtenti U");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }*/
    eseguiDetachShm(puntatoreSharedMemoryLibroMastro, "puntatoreSharedMemoryLibroMastro U");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiNodi);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryTutteCodeMessaggi");
        exit(EXIT_FAILURE);
    }*/
    eseguiDetachShm(puntatoreSharedMemoryTuttiNodi, "puntatoreSharedMemoryTuttiNodi U");
}

void avvisaPoiCambiaStato()
{
    puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].stato = USER_KO;
    user = UTENTE_STOP;

    return;
}

/*restiruisce la posizione o indice del nodo amico nellla shared memory principale*/
int scegliAmicoNodo(int idNodo)
{
    int numAmici;
    int amicoRandom;
#if (ENABLE_TEST)
    printf("Nodo arrivato e' %d\n", idNodo);
#endif
    numAmici = puntatoreSharedMemoryAmiciNodi[(idNodo - 1) * (getSoFriendsNum() * 2 + 1)];
#if (ENABLE_TEST)
    printf("Questo nodo ha %d amici:\n", numAmici);
    printf("%d e ha scelto a chi inviare %d che ha pid %d\n", puntatoreSharedMemoryTuttiNodi[idNodo].nodoPid, amicoRandom, puntatoreSharedMemoryTuttiNodi[puntatoreSharedMemoryAmiciNodi[(idNodo - 1) * (getSoFriendsNum() * 2 + 1) + amicoRandom]].nodoPid);
#endif
    amicoRandom = scegliNumeroNodo(numAmici);
    return puntatoreSharedMemoryAmiciNodi[(idNodo - 1) * (getSoFriendsNum() * 2 + 1) + amicoRandom];
    /*sleep(1000);*/
    /*puntatoreSharedMemoryAmiciNodi[idNodo * (getSoFriendsNum()*2 + 1) + scegliNumeroNodo(puntatoreSharedMemoryAmiciNodi[idNodo * (getSoFriendsNum()*2 + 1)])];*/
}