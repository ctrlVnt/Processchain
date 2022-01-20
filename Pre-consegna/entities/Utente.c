#include "../headers/process_chain_with_utils.h"
#include "../headers/utente.h"
#include "../headers/utente_master.h"

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

/*VARIABILI GLOBALI*/
/*ID della SM degli'ID delle CODE di messaggi alla quale devo fare l'attach*/
int idSharedMemoryTuttiNodi;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente gli id di tutte le code di messaggi, attenzione primo campo e' HEADER*/
nodo *puntatoreSharedMemoryTuttiNodi;
/*Id del semaforo che regola l'accesso alla MQ associata al nodo*/
int idSemaforoAccessoCodeMessaggi;
/*Id della coda di messaggi del processo master*/
int idCodaMessaggiProcessoMaster;

/*Il mio numero d'ordine*/
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
int soRetryIniziale;

/*variabili strutture per gestire i segnali*/
/*sigaction da settare per gestire ALARM - timer della simulazione*/
struct sigaction sigactionSigusr1Nuova;
/*sigaction dove memorizzare lo stato precedente*/
struct sigaction sigactionSigusr1Precedente;
/*sigaction generica - uso generale*/
struct sigaction actNuova;
/*old sigaction generica - uso generale*/
struct sigaction actPrecedente;
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

/*variabli ausiliari*/
/*variabile usata come secondo parametro nella funzione strtol, come puntatore dove la conversionbe della stringa ha avuto fine*/
char *endptr;
int shmdtRisposta;
int msgsndRisposta;
transazione transazioneInvio;
message messaggio;
int indiceLibroMastroRiservato;
struct timespec timespecRand;

/*FUNZIONI*/
void sigusr1Handler(int sigNum);
void sigusr2Handler(int sigNum);
int scegliUtenteRandom(int max);
int getQuantitaRandom(int max);
int getBudgetUtente(int indiceLibroMastroRiservato);
int inviaTransazione(int flag);
void avvisaPoiCambiaStato();
void interruptHandler(int sigNum);

int main(int argc, char const *argv[])
{
    /*variabili locali*/
    int q;
    int idCoda;
    int semopRisposta;

    indiceLibroMastroRiservato = 0;
    setSoMinTransGenNsec(parseLongFromParameters(argv, 1));
    setSoMaxTransGenNsec(parseLongFromParameters(argv, 2));

    /*ID DELLA SH DI TUTTE LE CODE DI MESSAGGI*/
    idSharedMemoryTuttiNodi = parseIntFromParameters(argv, 3);

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryTuttiNodi = (nodo *)shmat(idSharedMemoryTuttiNodi, NULL, 0);
    if (errno == EINVAL)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    /*Recupero il mio numero d'ordine*/
    numeroOrdine = parseIntFromParameters(argv, 4);
    idSharedMemoryLibroMastro = parseIntFromParameters(argv, 5);

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryLibroMastro = (transazione *)shmat(idSharedMemoryLibroMastro, NULL, 0);
    if (errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
    setSoRetry(parseIntFromParameters(argv, 6));
    setSoBudgetInit(parseIntFromParameters(argv, 7));
    idSharedMemoryTuttiUtenti = parseIntFromParameters(argv, 8);

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryTuttiUtenti = (utente *)shmat(idSharedMemoryTuttiUtenti, NULL, 0);
    if (errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }
    numeroTotaleUtenti = puntatoreSharedMemoryTuttiUtenti[0].userPid;
    idSemaforoAccessoCodeMessaggi = parseIntFromParameters(argv, 9);

    /*Mi atacco alla SM che contiene l'indice del libro mastro*/
    idSharedMemoryIndiceLibroMastro = parseIntFromParameters(argv, 10);

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryIndiceLibroMastro = (int *)shmat(idSharedMemoryIndiceLibroMastro, NULL, 0);

    setSoReward(parseIntFromParameters(argv, 11));
    setSoFriendsNum(parseIntFromParameters(argv, 12));
    setSoHops(parseIntFromParameters(argv, 13));

    idSharedMemoryAmiciNodi = parseIntFromParameters(argv, 14);

    puntatoreSharedMemoryAmiciNodi = (int *)shmat(idSharedMemoryAmiciNodi, NULL, 0);
    if (*puntatoreSharedMemoryAmiciNodi == -1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    idCodaMessaggiProcessoMaster = parseIntFromParameters(argv, 15);
    idSemaforoLimiteRisorse = parseIntFromParameters(argv, 16);

    /*imposto gli handler*/
    impostaHandlerSaWithMask(&sigactionSigusr1Nuova, &maskSetForSigusr1, &sigactionSigusr1Precedente, SIGUSR1, sigusr1Handler, 1, SIGUSR2);
    impostaHandlerSaNoMask(&sigactionSigusr2Nuova, &sigactionSigusr2Precedente, SIGUSR2, sigusr2Handler);
    impostaHandlerSaNoMask(&actNuova, &actPrecedente, SIGINT, interruptHandler);

    /*ciclo di vita utente*/
    user = UTENTE_CONTINUE;
    alrm = ALARM_CONTINUE;
    transazioneInvio.sender = getpid();
    limiteSistema = 1;
    soRetryIniziale = getSoRetry();

    while (user == UTENTE_CONTINUE && alrm == ALARM_CONTINUE)
    {
        switch (inviaTransazione(0))
        {
        case UTENTE_CONTINUE:
            if (getSoRetry() <= 0)
            {
                avvisaPoiCambiaStato();
            }
            break;
        case UTENTE_STOP:
            avvisaPoiCambiaStato();
            break;
        default:
            avvisaPoiCambiaStato();
            break;
        }
    }

    /*Deallocazione delle risorse*/
    eseguiDetachShm(puntatoreSharedMemoryAmiciNodi, "puntatoreSharedMemoryAmiciNodi U");
    eseguiDetachShm(puntatoreSharedMemoryIndiceLibroMastro, "puntatoreSharedMemoryIndiceLibroMastro U");
    eseguiDetachShm(puntatoreSharedMemoryTuttiUtenti, "puntatoreSharedMemoryTuttiUtenti U");
    eseguiDetachShm(puntatoreSharedMemoryLibroMastro, "puntatoreSharedMemoryLibroMastro U");
    eseguiDetachShm(puntatoreSharedMemoryTuttiNodi, "puntatoreSharedMemoryTuttiNodi U");

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
    alrm = ALLARME_SCATTATO;
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
            if (puntatoreSharedMemoryTuttiUtenti[iRandom].userPid != getpid() && puntatoreSharedMemoryTuttiUtenti[iRandom].stato != USER_KO)
            {
                return pidRandom = puntatoreSharedMemoryTuttiUtenti[iRandom].userPid;
            }
        }
    } while (alrm != ALLARME_SCATTATO);
    return pidRandom;
}

int getQuantitaRandom(int max)
{
    int result;
    clock_gettime(CLOCK_REALTIME, &timespecRand);
    srand(timespecRand.tv_nsec);
    result = rand() % (max - 2 + 1) + 2;

    if (result >= (max / 2) && max > 3)
    {
        result = result / 2;
    }

    return result;
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
    return indiceLibroMastroRiservato;
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
    indiceLibroMastroRiservato = getBudgetUtente(indiceLibroMastroRiservato);
    t.receiver = scegliUtenteRandom(numeroTotaleUtenti);
    t.reward = 0;
    /*imposto alcuni campi che so apriori*/
    m.mtype = getpid();
    m.hops = getSoHops();
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
                    idNodo = scegliAmicoNodo(idNodo, puntatoreSharedMemoryAmiciNodi);
                    if (semReserve(idSemaforoAccessoCodeMessaggi, -1, (idNodo - 1), IPC_NOWAIT) == -1 && errno == EAGAIN)
                    {
                        m.hops--;
                        attesaNonAttiva(getSoMinTransGenNsec(), getSoMaxTransGenNsec());
                        /*ricalcolo il budget*/
                        indiceLibroMastroRiservato = getBudgetUtente(indiceLibroMastroRiservato);
                    }
                    else
                    {
                        risposta = msgsnd(puntatoreSharedMemoryTuttiNodi[idNodo].mqId, &m, sizeof(m.transazione) + sizeof(m.hops), 0);
                        if (risposta != -1)
                        {
                            puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget -= q;
                            setSoRetry(soRetryIniziale);
                        }
                        break; /*esco dal ciclo*/
                    }
                } while (m.hops > 0 && alrm != ALLARME_SCATTATO);

                if (m.hops == 0 && limiteSistema == 1)
                {
                    /*devo delegare la transazione al master*/
                    errno = 0;
                    risposta = semReserve(idSemaforoLimiteRisorse, -1, 0, IPC_NOWAIT);
                    if (risposta != -1 && errno != EAGAIN)
                    {
                        risposta = msgsnd(idCodaMessaggiProcessoMaster, &m, sizeof(transazione) + sizeof(int), 0);
                        if (risposta != -1)
                        {
                            puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget -= q;
                            setSoRetry(soRetryIniziale);
                        }
                    }
                    else
                    {
                        limiteSistema = 0;
                    }
                }
                else
                {
                    if (limiteSistema == 0)
                    {
                        attesaNonAttiva(getSoMinTransGenNsec(), getSoMaxTransGenNsec());
                    }
                }
                attesaNonAttiva(2 * getSoMinTransGenNsec(), 2 * getSoMaxTransGenNsec());
            }
            /*So che c'e' almeno una cella libera nella TP*/
            else
            {
                /*invio la transazione*/
                risposta = msgsnd(puntatoreSharedMemoryTuttiNodi[idNodo].mqId, &m, sizeof(m.transazione) + sizeof(m.hops), 0);
                if (risposta == -1 && errno == EAGAIN)
                {
                    setSoRetry(getSoRetry() - 1);
                    attesaNonAttiva(getSoMinTransGenNsec(), getSoMaxTransGenNsec());
                    /*ricalcolo il budget*/
                    indiceLibroMastroRiservato = getBudgetUtente(indiceLibroMastroRiservato);
                }
                else if (risposta != -1)
                {
                    /*aggiorno budget*/
                    puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].budget -= (q);
                    setSoRetry(soRetryIniziale);
                    /*attesa non attiva*/
                    attesaNonAttiva(getSoMinTransGenNsec(), getSoMaxTransGenNsec());
                }
            }
        }
        else
        {
            /*caso budget insufficiente*/
            setSoRetry(getSoRetry() - 1);
            indiceLibroMastroRiservato = getBudgetUtente(indiceLibroMastroRiservato);
            attesaNonAttiva(getSoMinTransGenNsec(), getSoMaxTransGenNsec());
            /*ricalcolo il budget*/
            indiceLibroMastroRiservato = getBudgetUtente(indiceLibroMastroRiservato);
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

void avvisaPoiCambiaStato()
{
    puntatoreSharedMemoryTuttiUtenti[numeroOrdine + 1].stato = USER_KO;
    user = UTENTE_STOP;

    return;
}

void interruptHandler(int sigNum)
{
    user = UTENTE_STOP;
}
