#include "../headers/process_chain_with_utils.h"
#include "../headers/nodo.h"

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
7.soHops
8.ID semaforo dell'indice libro mastro
9.ID semaforo Mia coda di messaggi
10. soRegistrySize
11. SO_FRINEDS_NUM
12. idSharedMemoryAmiciNodi
13. idCodaMessaggiProcessoMaster
*/
/****************************************/

/*VARIABILI GLOBALI*/
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
/*Id della coda di messaggi del processo mastro*/
int idCodaMessaggiProcessoMaster;
/*ID della SM dove si trova il libro mastro*/
int idSharedMemoryLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria dove si trova effettivamente il libro mastro*/
transazione *puntatoreSharedMemoryLibroMastro;
/*ID SM dove si trova l'indice del libro mastro*/
int idSharedMemoryIndiceLibroMastro;
/*semaforo che regola l'accesso ai limiti delle risorse*/
int idSemaforoLimiteRisorse;
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
int soHops;
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
/**/
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
int limiteRisorse;

/*variabili strutture per gestire segnali*/

struct sigaction sigactionSigusr1Nuova;
struct sigaction sigactionSigusr1Precedente;
sigset_t maskSetForSigusr1;

/*sigaction generica - uso generale*/
struct sigaction actNuova;
/*old sigaction generica - uso generale*/
struct sigaction actPrecedente;

/*funzioni relative al nodo*/
void parseThenStoreParameter(int *dest, char const *argv[], int num, int flag);
/*gesrtione del segnale*/
void sigusr1Handler(int sigNum);
void aggiornaBilancioNodo(int indiceLibroMastroRiservato);
void interruptHandler(int sigNum);

int main(int argc, char const *argv[])
{
    /**/
    int idCoda;
    int inviato;
    int msgsndRisposta;
    int mexAmici;
    int regalo;
    int regaloInviato;
    message messaggioInviato;
    mexAmici = 0;
    inviato = 1;
    limiteRisorse = 1;
    /**/
#if (ENABLE_TEST)
    printf("%s: nodo[%d] e ha ricevuto #%d parametri.\n", argv[0], getpid(), argc);
#endif
    /*LIMITI DI TEMPO*/
    /*Recupero primo parametro*/
    setSoMinTransProcNsec(parseLongFromParameters(argv, 1));
/*TEST*/
#if (ENABLE_TEST)
    printf("soMinTransProcNsec: %ld\n", getSoMinTransProcNsec());
#endif

    /*Recupero secondo parametro*/
    setSoMaxTransProcNsec(parseLongFromParameters(argv, 2));
/*TEST*/
#if (ENABLE_TEST)
    printf("soMaxTransProcNsec: %ld\n", getSoMaxTransProcNsec());
#endif
    /******************/

    /*ID DELLA SH DI TUTTE LE CODE DI MESSAGGI*/
    idSharedMemoryTuttiNodi = parseIntFromParameters(argv, 3);
/*TEST*/
#if (ENABLE_TEST)
    printf("idSharedMemoryTuttiNodi: %d\n", idSharedMemoryTuttiNodi);
#endif

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryTuttiNodi = (nodo *)shmat(idSharedMemoryTuttiNodi, NULL, 0);
    if (errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }
    /*Recupero il mio numero d'ordine*/
    numeroOrdine = parseIntFromParameters(argv, 4);
/*numeroOrdine = (int)strtol(argv[4], &endptr,10);
    /*TEST*/
#if (ENABLE_TEST)
    printf("numeroOrdine: %d\n", numeroOrdine);
    printf("ID mia coda di messaggi - %d\n", puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].mqId);
#endif
    idCoda = numeroOrdine;

    idSharedMemoryLibroMastro = parseIntFromParameters(argv, 5);
/*TEST*/
#if (ENABLE_TEST)
    printf("idSharedMemoryLibroMastro: %d\n", idSharedMemoryLibroMastro);
#endif

    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryLibroMastro = (transazione *)shmat(idSharedMemoryLibroMastro, NULL, 0);
    if (errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }

    idSharedMemoryIndiceLibroMastro = parseIntFromParameters(argv, 6);
/*TEST*/
#if (ENABLE_TEST)
    printf("idSharedMemoryIndiceLibroMastro: %d\n", idSharedMemoryIndiceLibroMastro);
#endif
    /*ID OK -- effettuo l'attach*/
    puntatoreSharedMemoryIndiceLibroMastro = (int *)shmat(idSharedMemoryIndiceLibroMastro, NULL, 0);
    if (*puntatoreSharedMemoryIndiceLibroMastro == -1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    setSoHops(parseIntFromParameters(argv, 7));
/*TEST*/
#if (ENABLE_TEST)
    printf("soHops: %d\n", getSoHops());
#endif

    idSemaforoAccessoIndiceLibroMastro = parseIntFromParameters(argv, 8);
/*TEST*/
#if (ENABLE_TEST)
    printf("idSemaforoAccessoIndiceLibroMastro: %d\n", idSemaforoAccessoIndiceLibroMastro);
#endif

    idSemaforoAccessoMiaCodaMessaggi = parseIntFromParameters(argv, 9);
/*TEST*/
#if (ENABLE_TEST)
    printf("idSemaforoAccessoMiaCodaMessaggi: %d\n", idSemaforoAccessoMiaCodaMessaggi);
#endif

#if (ENABLE_TEST)
    printf(" N SO_TP_SIZE1: %d\n", getTpSize());
#endif
#if (ENABLE_TEST)
    printf("N SO_BLOCK_SIZE1: %d\n", getSoBlockSize());
#endif

    setSoRegistrySize(parseIntFromParameters(argv, 10));
#if (ENABLE_TEST)
    printf("N soRegistrySize: %d\n", getSoRegistrySize());
#endif
    setSoFriendsNum(parseIntFromParameters(argv, 11));
#if (ENABLE_TEST)
    printf("N soNumFriends: %d\n", getSoFriendsNum());
#endif

    idSharedMemoryAmiciNodi = parseIntFromParameters(argv, 12);
#if (ENABLE_TEST)
    printf("N idSharedMemoryAmiciNodi: %d\n", idSharedMemoryAmiciNodi);
#endif
    puntatoreSharedMemoryAmiciNodi = (int *)shmat(idSharedMemoryAmiciNodi, NULL, 0);
    if (*puntatoreSharedMemoryAmiciNodi == -1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    idCodaMessaggiProcessoMaster = parseIntFromParameters(argv, 13);
#if (ENABLE_TEST)
    printf("N idCodaMessaggiProcessoMaster: %d\n", idCodaMessaggiProcessoMaster);
#endif
    idSemaforoLimiteRisorse = parseIntFromParameters(argv, 14);
#if (ENABLE_TEST)
    printf("N idCodaMessaggiProcessoMaster: %d\n", idCodaMessaggiProcessoMaster);
#endif

    /*imposto l'handler*/
    /*sigactionSigusr1Nuova.sa_flags = 0;
    sigemptyset(&sigactionSigusr1Nuova.sa_mask);
    sigactionSigusr1Nuova.sa_handler = sigusr1Handler;
    sigaction(SIGUSR1, &sigactionSigusr1Nuova, &sigactionSigusr1Precedente);*/
    /**/
    impostaHandlerSaNoMask(&sigactionSigusr1Nuova, &sigactionSigusr1Precedente, SIGUSR1, sigusr1Handler);

    /*POSSO INIZIARE A GESTIRE LE TRANSAZIONI*/

    /*printf("Imposto SIGINT a %d\n", impostaHandlerSaNoMask(&actNuova, &actPrecedente, SIGINT, interruptHandler));*/

    node = NODO_CONTINUE;
    tpPiena = 0;
    /*alloco dinamicamente la TP e blocco*/
    transactionPool = (transazione *)calloc(getTpSize(), sizeof(transazione));
    if (errno == EINVAL)
    {
        perror("calloco transaction pool");
        exit(EXIT_FAILURE);
    }
    blocco = (transazione *)calloc(getSoBlockSize(), sizeof(transazione));
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
    regalo = 0;
    /*ciclo di vita del nodo*/
    while (node)
    {
        /*medianto il seguente ciclo, posso riempire la TP, e nel caso mi sia possibile inviare un blocco, lo interrompo e proseguo con l'invio del blocco*/
        while (tpPiena < getTpSize() && node == NODO_CONTINUE && riempimentoTpStop)
        {
            errno = 0;
            /*ricevo la transazione dalla MQ associata al nodo*/
            msgrcvRisposta = msgrcv(puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].mqId, &messaggioRicevuto, sizeof(messaggioRicevuto.transazione) + sizeof(messaggioRicevuto.hops), 0, IPC_NOWAIT);

#if (ENABLE_TEST)
            printf("[%d] ha effettuato la msgrcv dalla coda ID[%d] con risposta: %d\n", getpid(), puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].mqId, msgrcvRisposta);
#endif
            /**/

            /**/
            if (errno != ENOMSG && msgrcvRisposta != -1)
            {
                if (messaggioRicevuto.mtype == 5)
                {
                    mexAmici++;
                }
                /*MODIFICA6GEN*/
                /*se messaggio e' valido, incremento il contatore di riempimento della TP*/
                tpPiena++;
                /*incremento il contatore che regola la possibilita' di riempire il blocco*/
                bloccoRiempibile++;
                /*inmaggazzino la transazione*/
                transactionPool[(indiceSuccessivoTransactionPool++)] = messaggioRicevuto.transazione;
                /*incremento il valore di "transazioni pendenti"*/
                puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].transazioniPendenti = tpPiena;
                /**/

                /*se tp e' piena - comincio un altro ciclo di scrittura sulla tp*/
                if (indiceSuccessivoTransactionPool == getTpSize())
                {
                    /*reimposto il puntatore alla prima cella della TP*/
                    indiceSuccessivoTransactionPool = 0;
                }
            }
            else
            {
                errno = 0;
                /*se e' possibile inviare un blocco - lo faccio*/
                if (bloccoRiempibile >= getSoBlockSize())
                {
                    /*flag che ferma la ricezione - eventuali transazioni */
                    riempimentoTpStop = 0;
                }
                else
                {
                    attesaNonAttiva(getSoMinTransProcNsec(), getSoMaxTransProcNsec());
                }
            }
        }
        /*se e' scattato l'alarm esco e dealloco le risorse utilizzate*/
        if (node == NODO_STOP)
        {
            break; /*esco dal ciclo esterno*/
        }

        /*
            INVIO TRANSAZIONE DI REGALO
        */
        if (1) /*variabile invioAmico risulta FORSE superficiale*/
        {
            messaggioInviato.mtype = 5;
            messaggioInviato.transazione = transactionPool[(indiceSuccessivoBlocco++)];
            messaggioInviato.hops = getSoHops();

            if (indiceSuccessivoBlocco == getTpSize())
            {
                indiceSuccessivoBlocco = 0;
            }

#if (ENABLE_TEST)
            printf("\nHOPS NODO MESSAGGIO RICEVUTO: %d\n", messaggioInviato.hops);
#endif
            idCoda = numeroOrdine + 1;
            /**/
            regaloInviato = 1;
            /**/
            do
            {
                idCoda = scegliAmicoNodo(idCoda, puntatoreSharedMemoryAmiciNodi);

#if (ENABLE_TEST)
                printf("AMICO TROVATO CON IDCODA: %d\n", idCoda);
#endif
                semopRisposta = semReserve(idSemaforoAccessoMiaCodaMessaggi, -1, (idCoda - 1), IPC_NOWAIT);

                if (semopRisposta != -1)
                {
                    msgsnd(puntatoreSharedMemoryTuttiNodi[idCoda].mqId, &messaggioInviato, sizeof(messaggioInviato.transazione) + sizeof(messaggioInviato.hops), 0);
                    /*faccio release sul mio semaforo dopo aver inviato la transazione di regalo*/
                    semRelease(idSemaforoAccessoMiaCodaMessaggi, 1, numeroOrdine, 0);
                    regaloInviato = 0;
                    tpPiena--;
                    bloccoRiempibile = tpPiena;
                    /*TRANSAZIONE INVIATA ALL'AMICO*/
                    attesaNonAttiva(getSoMinTransProcNsec(), getSoMaxTransProcNsec());
                }
                else
                {
                    messaggioInviato.hops--;
                    attesaNonAttiva(getSoMinTransProcNsec(), getSoMaxTransProcNsec());
                    if (messaggioInviato.hops == 0)
                    {
                        semopRisposta = semReserve(idSemaforoLimiteRisorse, -1, 0, IPC_NOWAIT);
                        if (semopRisposta != -1)
                        {
                            msgsnd(idCodaMessaggiProcessoMaster, &messaggioInviato, sizeof(messaggioInviato.transazione) + sizeof(messaggioInviato.hops), 0);
                            /*faccio release sul mio semaforo dopo aver inviato la transazione di regalo*/
                            semRelease(idSemaforoAccessoMiaCodaMessaggi, 1, numeroOrdine, 0);
                            regaloInviato = 0;
                            tpPiena--;
                            bloccoRiempibile = tpPiena;
                        }
                        else
                        {
                            /* printf("LIMITE RISORSE RAGGIUNTO -> NODO\n");*/
                            indiceSuccessivoBlocco--;
                            if (indiceSuccessivoBlocco == -1)
                            {
                                indiceSuccessivoBlocco = SO_TP_SIZE - 1;
                            }
                            regaloInviato = 0;
                        }
                    }
                }
            } while (/*semopRisposta == -1*/ regaloInviato && node);

            regaloInviato = 0;
        }
        /**/

        /*riempimneto del blocco*/
        for (i = 0; i < (getSoBlockSize() - 1); i++)
        {
            blocco[i] = transactionPool[(indiceSuccessivoBlocco++)];
            transazioneReward.quantita += blocco[i].reward;
            if (indiceSuccessivoBlocco == getTpSize())
            {
                indiceSuccessivoBlocco = 0;
            }
            /*if(puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].transazioniPendenti < 0)
            {
                printf("\n\n\n\n\n\n\nNEGATIVO %d\n\n\n\n\n\n\n\n\n", puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].transazioniPendenti);
            }*/
            /*printf("%d\n", puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].transazioniPendenti);*/
        }
        clock_gettime(CLOCK_REALTIME, &transazioneReward.timestamp);
        blocco[getSoBlockSize() - 1] = transazioneReward;
        transazioneReward.quantita = 0;
/*TEST*/
#if (ENABLE_TEST)
        printf("[%d] Ho costruito il blocco: quanita' di reward e' %d\n", getpid(), transazioneReward.quantita);
#endif
        /*devo riservare l'indice del libro mastro*/
        /*tentativo di riservare l'indice*/

        /*release dell'indice libro mastro*/

        /*se l'indice supera la capacita' del libro mastro - ciclo di vita termina*/
        attesaNonAttiva(getSoMinTransProcNsec(), getSoMaxTransProcNsec());
        /**/
        semopRisposta = semReserve(idSemaforoAccessoIndiceLibroMastro, -1, 0, 0);
        indiceLibroMastroRiservato = *(puntatoreSharedMemoryIndiceLibroMastro);
        if (indiceLibroMastroRiservato >= getSoRegistrySize()) /*11 parametro settato*/
        {
            semopRisposta = semRelease(idSemaforoAccessoIndiceLibroMastro, 1, 0, 0);
            node = NODO_STOP;
            break;
        }
        /*ho riservato l'indice - posso scrivere sul libro mastro*/
        for (i = 0; i < getSoBlockSize(); i++)
        {
            puntatoreSharedMemoryLibroMastro[indiceLibroMastroRiservato * getSoBlockSize() + i] = blocco[i];
        }
        *(puntatoreSharedMemoryIndiceLibroMastro) += 1;
        semopRisposta = semRelease(idSemaforoAccessoIndiceLibroMastro, 1, 0, 0);
/*TEST*/
#if (ENABLE_TEST)
        printf("[%d] ho finito di scrivere sul libro mastro\n", getpid());
#endif
        /*rilascio il semaforo che contiene blocksize - 1 transazione del blocco + la transazione di regalo*/
        semopRisposta = semRelease(idSemaforoAccessoMiaCodaMessaggi, (getSoBlockSize() - 1), numeroOrdine, 0);
        /**/
        tpPiena -= (getSoBlockSize() - 1);
        bloccoRiempibile = tpPiena;
        riempimentoTpStop = 1;
        regalo = 1;
        puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].transazioniPendenti = tpPiena;
        /*puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].transazioniPendenti -= (getSoBlockSize() - 1);*/

        /*aggiorno bilancio del nodo*/
        aggiornaBilancioNodo(indiceLibroMastroRiservato);
        /*USARE VARIABILE PER INVIO TRANSAZIONE A NODI AMICI, RIMETTO A ZERO
        invioAdAmico = 1;*/
    }

/*****************************************/
/*STAMPO IL NUMERO DI AMICI RICEVUTI*/
#if (ENABLE_TEST)
    printf("\n\n\nHo ricevuto: %d transazioni da 'AMICI'\n\n\n.\n", mexAmici);
#endif

    /*CICLO DI VITA DEL NODO TERMINA*/
    /*Deallocazione delle risorse*/
    free(transactionPool);
    free(blocco);
    eseguiDetachShm(puntatoreSharedMemoryAmiciNodi, "puntatoreSharedMemoryAmiciNodi N");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryAmiciNodi);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryAmiciNodi");
        exit(EXIT_FAILURE);
    }*/
    eseguiDetachShm(puntatoreSharedMemoryIndiceLibroMastro, "puntatoreSharedMemoryIndiceLibroMastro N");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryIndiceLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }*/
    eseguiDetachShm(puntatoreSharedMemoryLibroMastro, "puntatoreSharedMemoryLibroMastro N");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }*/
    eseguiDetachShm(puntatoreSharedMemoryTuttiNodi, "puntatoreSharedMemoryTuttiNodi N");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiNodi);
    if (shmdtRisposta == -1)
    {
        perror("shmdt puntatoreSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }*/
#if (ENABLE_TEST)
    printf("Risorse deallocate correttamente\n");
#endif
    return EXIT_SUCCESS;
}

void sigusr1Handler(int sigNum)
{
#if (ENABLE_TEST)
    printf("NODO SIGUSR1 ricevuto\n");
#endif
    node = NODO_STOP;
}

/*visto che lavoriamo con le SM, provare a delegare questa operazione al libro mastro*/
void aggiornaBilancioNodo(int indiceLibroMastroRiservato)
{
    int ultimoIndice;
    ultimoIndice = *(puntatoreSharedMemoryIndiceLibroMastro);

    for (indiceLibroMastroRiservato; indiceLibroMastroRiservato < ultimoIndice; indiceLibroMastroRiservato++)
    {
        if (puntatoreSharedMemoryLibroMastro[indiceLibroMastroRiservato * getSoBlockSize() + (getSoBlockSize() - 1)].receiver == getpid())
        {
            puntatoreSharedMemoryTuttiNodi[numeroOrdine + 1].budget += puntatoreSharedMemoryLibroMastro[indiceLibroMastroRiservato * getSoBlockSize() + (getSoBlockSize() - 1)].quantita;
        }
    }
/*TEST*/
#if (ENABLE_TEST)
    printf("[%d] aggiornamento bilancio terminato\n", getpid());
#endif
}

void interruptHandler(int sigNum)
{
    node = NODO_STOP;
}
