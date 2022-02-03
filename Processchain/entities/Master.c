#include "../headers/process_chain.h"
#include "../headers/master.h"
#include "../headers/utente_master.h"
#include "../headers/customs.h"

/*ID SM*/
/*ID della SM contenente il libro mastro*/
int idSharedMemoryLibroMastro;
/*ID SM dove si trova l'indice del libro mastro*/
int idSharedMemoryIndiceLibroMastro;
/*ID della SM degli'ID delle CODE di messaggi alla quale devo fare l'attach*/
int idSharedMemoryTuttiNodi;
/*ID della SM contenente tutti i PID dei processi utenti - visti come destinatari*/
int idSharedMemoryTuttiUtenti;
/*ID della SM che contiene i NODE_FRIENDS di ciascun nodo*/
int idSharedMemoryAmiciNodi;

/*Puntatori SM*/
/*Dopo l'attach, punta alla porzione di memoria dove si trova effettivamente il libro mastro*/
transazione *puntatoreSharedMemoryLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria condivisa dove si trova effettivamente l'indice del libro mastro*/
int *puntatoreSharedMemoryIndiceLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente gli id di tutte le code di messaggi, attenzione primo campo e' HEADER*/
nodo *puntatoreSharedMemoryTuttiNodi;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente i PID degli utenti*/
utente *puntatoreSharedMemoryTuttiUtenti;
/*Dopo l'attach punta alla porzione di memoria dove si trovano gli amici del nodo*/
int *puntatoreSharedMemoryAmiciNodi;
/*id semaforo limite risorse*/
int idSemaforoLimiteRisorse;

/*Semafori*/
/*Id del semaforo che regola l'accesso all'indice.
Il semaforo di tipo binario.*/
int idSemaforoAccessoIndiceLibroMastro;
/*
Id del semaforo che regola l'accesso alla Msgqueue associata al nodo
Il valore iniziale e' pari alla capacita' della transaction pool.
*/
int idSemaforoAccessoCodeMessaggi;
/*Id del semaforo per sincronizzare l'avvio dei processi*/
int idSemaforoSincronizzazioneTuttiProcessi;

/*variabile struttura per effettuare le operazioni sul semaforo*/
/*streuttura generica per dialogare con semafori IPC V*/
struct sembuf operazioniSemaforo;
/*array contenente i valori iniziali di ciascun semaforo associato ad ogni MQ*/
unsigned short *arrayValoriInizialiSemaforiCodeMessaggi;
/*struttura utilizzata nella semctl*/
union semun semaforoUnion;

/*ID MQ del processo MASTER*/
int idCodaMessaggiProcessoMaster;

/*variabili strutture per gestire i segnali*/
/*sigaction da settare per gestire ALARM - timer della simulazione*/
struct sigaction sigactionAlarmNuova;
/*sigaction dove memorizzare lo stato precedente*/
struct sigaction sigactionAlarmPrecedente;
/*maschera per SIGALARM*/
/*sigaction generica - uso generale*/
struct sigaction actNuova;
/*precedente sigaction generica - uso generale*/
struct sigaction actPrecedente;

/*Variabile messaggio prelevato da coda di messaggi del master*/
message m;

/*variabili ausiliari*/
int readAllParametersRisposta;
int shmctlRisposta;
int shmdtRisposta;
int semopRisposta;
int semctlRisposta;
int msggetRisposta;
int msgctlRisposta;
int execRisposta;
int i;
int j;
int nextFriend;
/*int nuovoNodoCreato;*/
int nuovoNodoCont;
/*regola il ciclo di vita del master*/
int master;
int childPid;
int childPidWait;
int childStatus;
int motivoTerminazione;
int indiceAmicoSuccessivo;
int cont;

/*Variabili necessari per poter avviare i nodi, successivamente gli utenti*/
char parametriPerNodo[15][32];
char intToStrBuff[32];
char parametriPerUtente[17][32];

/*Coefficiente LIMITE RISORSE*/
int q;
int k;
int g;

/*VARIABILI PER LA STAMPA*/
utente utenteMax;
utente utenteMin;
nodo nodoMax;
nodo nodoMin;
utente *printUtenti;
nodo *printNodi;
int indiceStampaUtenti;
int indiceStampaNodi;
int indicePassaCelleLibroStampa;

/*FUNZIONI*/
/*Inizializza le variabili globali da un file di input TXT. */
int readAllParameters(const char *configFilePath);
/*gestione del segnale*/
void alarmHandler(int sigNum);
/*gestisce il segnale SIGINT*/
void interruptHandler(int sigNum);
/*stampa terminale*/
void stampaTerminale(int flag);
/*stampa il libro mastro in file di testo*/
void outputLibroMastro();
/*la funzione che aggiunge amici*/
void aggiungiAmico(int numeroOrdine);

int main(int argc, char const *argv[])
{
    printf("Sono MASTER[%d]\n", getpid());
    master = MASTER_CONTINUE;
    motivoTerminazione = -1;
    /*Parsing dei parametri a RUN-TIME*/
    readAllParametersRisposta = readAllParameters(argv[1]);
    if (readAllParametersRisposta == -1)
    {
        perror("- fetch parameters");
        exit(EXIT_FAILURE);
    }
    nextFriend = 0;
    
    /*assegno al massimo cento nodi creabili*/
    q = getSoSimSec() >= 100 ? 100 : getSoSimSec();

    /*Inizializzazione array per stampa*/
    printUtenti = (utente *)calloc(getSoUsersNum(), sizeof(utente));
    printNodi = (nodo *)calloc((q + getSoNodesNum()), sizeof(nodo));
    
    indiceStampaUtenti = 0;
    indiceStampaNodi = 0;

    /*Inizializzazione LIBRO MASTRO*/
    /*SM*/
    idSharedMemoryLibroMastro = shmget(IPC_PRIVATE, getSoRegistrySize() * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);
    if (idSharedMemoryLibroMastro == -1)
    {
        perror("- shmget idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }

    puntatoreSharedMemoryLibroMastro = (transazione *)shmat(idSharedMemoryLibroMastro, NULL, 0);

    if (errno == EINVAL)
    {
        perror("- shmat idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }

    /*Inizializzazione INDICE LIBRO MASTRO*/
    /*SM*/
    idSharedMemoryIndiceLibroMastro = shmget(IPC_PRIVATE, sizeof(int), 0600 | IPC_CREAT);
    if (idSharedMemoryIndiceLibroMastro == -1)
    {
        perror("- shmget idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }

    puntatoreSharedMemoryIndiceLibroMastro = (int *)shmat(idSharedMemoryIndiceLibroMastro, NULL, 0);
    if (*puntatoreSharedMemoryIndiceLibroMastro == -1)
    {
        perror("- shmat idSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }

    puntatoreSharedMemoryIndiceLibroMastro[0] = 0;

    /*SEMAFORO*/
    idSemaforoAccessoIndiceLibroMastro = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);

    if (idSemaforoAccessoIndiceLibroMastro == -1)
    {
        perror("- semget idSemaforoAccessoIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }

    semopRisposta = semRelease(idSemaforoAccessoIndiceLibroMastro, 1, 0, 0);

    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoAccessoIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    /*FINE Inizializzazione INDICE LIBRO MASTRO*/

    /*SM*/
    /*REMINDER: prima cella di questa SM indica il NUMERO totale di CODE presenti, quindi rispecchia anche il numero dei nodi presenti*/
    idSharedMemoryTuttiNodi = shmget(IPC_PRIVATE, sizeof(nodo) * (q + getSoNodesNum() + 1), 0600 | IPC_CREAT);
    if (idSharedMemoryTuttiNodi == -1)
    {
        perror("shmget idSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }

    puntatoreSharedMemoryTuttiNodi = (nodo *)shmat(idSharedMemoryTuttiNodi, NULL, 0);

    if (errno == EINVAL)
    {
        perror("- shmat idSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }

    /*creo coda di messaggi per processo Master*/
    idCodaMessaggiProcessoMaster = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
    if (idCodaMessaggiProcessoMaster == -1)
    {
        perror("- msgget tutteCodeMessaggi");
        exit(EXIT_FAILURE);
    }

    /*continee il numero totale di nodi*/
    puntatoreSharedMemoryTuttiNodi[0].nodoPid = getSoNodesNum();
    puntatoreSharedMemoryTuttiNodi[0].budget = -1;
    puntatoreSharedMemoryTuttiNodi[0].mqId = -1;
    puntatoreSharedMemoryTuttiNodi[0].transazioniPendenti = -1;

    for (i = 0; i < getSoNodesNum(); i++)
    {
        msggetRisposta = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
        if (msggetRisposta == -1)
        {
            perror("- msgget tutteCodeMessaggi");
            exit(EXIT_FAILURE);
        }
        puntatoreSharedMemoryTuttiNodi[i + 1].mqId = msggetRisposta;
        puntatoreSharedMemoryTuttiNodi[i + 1].transazioniPendenti = 0;
    }

    /*suppongo di non poter avere piu' di q + SO_FRIENDS_NUM +1 campo header amici*/
    idSharedMemoryAmiciNodi = shmget(IPC_PRIVATE, q + getSoNodesNum() * (getSoFriendsNum() * 2 + 1) * sizeof(int), 0600 | IPC_CREAT);
    if (idSharedMemoryAmiciNodi == -1)
    {
        perror("shmget idSharedMemoryAmiciNodi");
        exit(EXIT_FAILURE);
    }

    puntatoreSharedMemoryAmiciNodi = (int *)shmat(idSharedMemoryAmiciNodi, NULL, 0);

    if (errno == EINVAL)
    {
        perror("- shmat idSharedMemoryAmiciNodi");
        exit(EXIT_FAILURE);
    }

    /*setto ogni cella della shared memory a -1*/
    memset(puntatoreSharedMemoryAmiciNodi, -1, q + getSoNodesNum() * (getSoFriendsNum() * 2 + 1) * sizeof(int));

    /*SEMAFORI*/
    idSemaforoAccessoCodeMessaggi = semget(IPC_PRIVATE, q + getSoNodesNum(), 0600 | IPC_CREAT);

    if (idSemaforoAccessoCodeMessaggi == -1)
    {
        perror("- semget idSemaforoAccessoCodeMessaggi");
        exit(EXIT_FAILURE);
    }

    arrayValoriInizialiSemaforiCodeMessaggi = (unsigned short *)calloc(getSoNodesNum() * q, sizeof(unsigned short));

    if (arrayValoriInizialiSemaforiCodeMessaggi == NULL)
    {
        perror("- calloc arrayValoriInizialiSemaforiCodeMessaggi");
        exit(EXIT_FAILURE);
    }

    /*limite di sistema*/
    idSemaforoLimiteRisorse = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);

    if (idSemaforoLimiteRisorse == -1)
    {
        perror("- semget idSemaforoLimiteRisorse");
        exit(EXIT_FAILURE);
    }

    semaforoUnion.val = q + getSoNodesNum();

    semopRisposta = semctl(idSemaforoLimiteRisorse, 0, SETVAL, semaforoUnion);

    if (semopRisposta == -1)
    {
        perror("semctl semaforo risorse");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < getSoNodesNum() * q; i++)
    {
        arrayValoriInizialiSemaforiCodeMessaggi[i] = getTpSize();
    }

    semaforoUnion.array = arrayValoriInizialiSemaforiCodeMessaggi;
    semctlRisposta = semctl(idSemaforoAccessoCodeMessaggi, 0, SETALL, semaforoUnion);

    if (semctlRisposta == -1)
    {
        perror("- semctl SETALL");
        exit(EXIT_FAILURE);
    }

    /*FINE Inizializzazione SM delle MQ*/
    /*INIZIO Inizializzazione SM che contiene i PID degli USER*/
    idSharedMemoryTuttiUtenti = shmget(IPC_PRIVATE, (getSoUsersNum() + 1) * sizeof(utente), 0600 | IPC_CREAT);

    if (idSharedMemoryTuttiUtenti == -1)
    {
        perror("- shmget idSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }

    /*FINE Inizializzazione SM che contiene i PID degli USER*/

    /*INIZIO Semaforo di sincronizzazione*/

    idSemaforoSincronizzazioneTuttiProcessi = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);

    if (idSemaforoSincronizzazioneTuttiProcessi == -1)
    {
        perror("- semget idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }

    semopRisposta = semRelease(idSemaforoSincronizzazioneTuttiProcessi, (getSoNodesNum() + 1), 0, 0);

    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }

    /*CREAZIONE NODI*/
    /*LISTA DEI PARAMETRI COMUNE A TUTTI I NODI*/
    strcpy(parametriPerNodo[0], "NodoBozza.out");
    sprintf(intToStrBuff, "%ld", getSoMinTransProcNsec());
    strcpy(parametriPerNodo[1], intToStrBuff);
    sprintf(intToStrBuff, "%ld", getSoMaxTransProcNsec());
    strcpy(parametriPerNodo[2], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSharedMemoryTuttiNodi);
    strcpy(parametriPerNodo[3], intToStrBuff);
    /*Il quarto parametro e' quello variabile -- ID di ciascun nodo*/
    sprintf(intToStrBuff, "%d", idSharedMemoryLibroMastro);
    strcpy(parametriPerNodo[5], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSharedMemoryIndiceLibroMastro);
    strcpy(parametriPerNodo[6], intToStrBuff);
    sprintf(intToStrBuff, "%d", getSoHops());
    strcpy(parametriPerNodo[7], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSemaforoAccessoIndiceLibroMastro);
    strcpy(parametriPerNodo[8], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSemaforoAccessoCodeMessaggi);
    strcpy(parametriPerNodo[9], intToStrBuff);
    sprintf(intToStrBuff, "%d", getSoRegistrySize());
    strcpy(parametriPerNodo[10], intToStrBuff);
    sprintf(intToStrBuff, "%d", getSoFriendsNum());
    strcpy(parametriPerNodo[11], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSharedMemoryAmiciNodi);
    strcpy(parametriPerNodo[12], intToStrBuff);
    sprintf(intToStrBuff, "%d", idCodaMessaggiProcessoMaster);
    strcpy(parametriPerNodo[13], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSemaforoLimiteRisorse);
    strcpy(parametriPerNodo[14], intToStrBuff);

    for (i = 0; i < getSoNodesNum(); i++)
    {
        switch ((childPid = fork()))
        {
        case -1:
            perror("fork");
            master = MASTER_STOP;
            motivoTerminazione = LIMITE_SISTEMA;
            break;
        case 0:
            /*QUA HO EREDITATO TUTTI I PUNTATORI*/
            semopRisposta = semReserve(idSemaforoSincronizzazioneTuttiProcessi, -1, 0, 0);
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }
            semopRisposta = semReserve(idSemaforoSincronizzazioneTuttiProcessi, 0, 0, 0);
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }

            /*INIZIO Costruire la lista di parametri*/
            sprintf(intToStrBuff, "%d", /*i-esimo nodo*/ i);
            strcpy(parametriPerNodo[4], intToStrBuff);
            /*FINE Lista*/

            execRisposta = execlp("./NodoBozza.out", parametriPerNodo[0], parametriPerNodo[1], parametriPerNodo[2], parametriPerNodo[3], parametriPerNodo[4], parametriPerNodo[5], parametriPerNodo[6], parametriPerNodo[7], parametriPerNodo[8], parametriPerNodo[9], parametriPerNodo[10], parametriPerNodo[11], parametriPerNodo[12], parametriPerNodo[13], parametriPerNodo[14], NULL);

            if (execRisposta == -1)
            {
                perror("execlp");
                exit(EXIT_FAILURE);
            }
            break;

        default:
            puntatoreSharedMemoryTuttiNodi[i + 1].budget = 0;
            puntatoreSharedMemoryTuttiNodi[i + 1].nodoPid = childPid;
            printNodi[i].nodoPid = childPid;
            printNodi[i].budget = 0;
            break;
        }
    }

    /*assegno gli amici ai carissimi nodi*/
    indiceAmicoSuccessivo = 2;
    for (i = 0; i < getSoNodesNum(); i++)
    {
        puntatoreSharedMemoryAmiciNodi[i * (getSoFriendsNum() * 2 + 1)] = getSoFriendsNum();
        indiceAmicoSuccessivo = i + 2;
        if (indiceAmicoSuccessivo == getSoNodesNum() + 1)
        {
            indiceAmicoSuccessivo = 1;
        }
        for (j = 1; j <= getSoFriendsNum(); j++)
        {
            puntatoreSharedMemoryAmiciNodi[i * (getSoFriendsNum() * 2 + 1) + j] = (indiceAmicoSuccessivo++);
            if (indiceAmicoSuccessivo == getSoNodesNum() + 1)
            {
                indiceAmicoSuccessivo = 1;
            }
        }
    }

/*STAMPA AMICI ASSEGNATI*/
#if (ENABLE_TEST)
    for (k = 0; k < getSoNodesNum(); k++)
    {
        printf("NODO %d : \t%d\t|", (k + 1), puntatoreSharedMemoryAmiciNodi[k * (getSoFriendsNum() * 2 + 1)]);
        for (g = 1; g <= getSoFriendsNum() * 2; g++)
        {
            printf("%d\t", puntatoreSharedMemoryAmiciNodi[(k * (getSoFriendsNum() * 2 + 1)) + g]);
        }
        printf("\n");
    }
#endif

    /*FINE CREAZIONE NODI*/

    semopRisposta = semReserve(idSemaforoSincronizzazioneTuttiProcessi, -1, 0, 0);

    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
        exit(EXIT_FAILURE);
    }

    /*FINE Semaforo di sincronizzazione*/

    /*PARTE DEGLI UTENTI*/
    semopRisposta = semRelease(idSemaforoSincronizzazioneTuttiProcessi, (getSoUsersNum() + 1), 0, 0);

    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
        exit(EXIT_FAILURE);
    }

    i = 0;

    /*creo la SM dove memorizzo gli utenti*/
    puntatoreSharedMemoryTuttiUtenti = (utente *)shmat(idSharedMemoryTuttiUtenti, NULL, 0);

    if (errno == EINVAL)
    {
        perror("shmat puntatoreSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }

    puntatoreSharedMemoryTuttiUtenti[0].budget = -1;
    puntatoreSharedMemoryTuttiUtenti[0].stato = -1;
    puntatoreSharedMemoryTuttiUtenti[0].userPid = getSoUsersNum();

    /*COSTRUZIONE LISTA DEI PARAMETRI COMUNI A TUTTI GLI UTENTI*/
    strcpy(parametriPerUtente[0], "UtenteBozza.out");
    sprintf(intToStrBuff, "%ld", getSoMinTransGenNsec());
    strcpy(parametriPerUtente[1], intToStrBuff);
    sprintf(intToStrBuff, "%ld", getSoMaxTransGenNsec());
    strcpy(parametriPerUtente[2], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSharedMemoryTuttiNodi);
    strcpy(parametriPerUtente[3], intToStrBuff);
    /*IL QUARTO PARAMETRO E' QUELLO VARIABILE*/
    sprintf(intToStrBuff, "%d", idSharedMemoryLibroMastro);
    strcpy(parametriPerUtente[5], intToStrBuff);
    sprintf(intToStrBuff, "%d", getSoRetry());
    strcpy(parametriPerUtente[6], intToStrBuff);
    sprintf(intToStrBuff, "%d", getSoBudgetInit());
    strcpy(parametriPerUtente[7], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSharedMemoryTuttiUtenti);
    strcpy(parametriPerUtente[8], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSemaforoAccessoCodeMessaggi);
    strcpy(parametriPerUtente[9], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSharedMemoryIndiceLibroMastro);
    strcpy(parametriPerUtente[10], intToStrBuff);
    sprintf(intToStrBuff, "%d", getSoReward());
    strcpy(parametriPerUtente[11], intToStrBuff);
    sprintf(intToStrBuff, "%d", getSoFriendsNum());
    strcpy(parametriPerUtente[12], intToStrBuff);
    sprintf(intToStrBuff, "%d", getSoHops());
    strcpy(parametriPerUtente[13], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSharedMemoryAmiciNodi);
    strcpy(parametriPerUtente[14], intToStrBuff);
    sprintf(intToStrBuff, "%d", idCodaMessaggiProcessoMaster);
    strcpy(parametriPerUtente[15], intToStrBuff);
    sprintf(intToStrBuff, "%d", idSemaforoLimiteRisorse);
    strcpy(parametriPerUtente[16], intToStrBuff);


    for (i = 0; i < getSoUsersNum(); i++)
    {
        switch (childPid = fork())
        {
        case -1:
            perror("fork");
            master = MASTER_STOP;
            motivoTerminazione = LIMITE_SISTEMA;
            break;
        case 0:
            /*QUA HO EREDITATO TUTTI I PUNTATORI*/
            /*devo notificare il parent e attendere lo zero*/

            semopRisposta = semReserve(idSemaforoSincronizzazioneTuttiProcessi, -1, 0, 0);

            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }
            
            semopRisposta = semReserve(idSemaforoSincronizzazioneTuttiProcessi, 0, 0, 0);
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }

            /* UNICO PARAMETRO VARIABILE E' l'ID del nodo */
            sprintf(intToStrBuff, "%d", i);
            strcpy(parametriPerUtente[4], intToStrBuff);

            execRisposta = execlp("./UtenteBozza.out", parametriPerUtente[0], parametriPerUtente[1], parametriPerUtente[2], parametriPerUtente[3], parametriPerUtente[4], parametriPerUtente[5], parametriPerUtente[6], parametriPerUtente[7], parametriPerUtente[8], parametriPerUtente[9], parametriPerUtente[10], parametriPerUtente[11], parametriPerUtente[12], parametriPerUtente[13], parametriPerUtente[14], parametriPerUtente[15], parametriPerUtente[16], NULL);

            if (execRisposta == -1)
            {
                perror("execlp");
                exit(EXIT_FAILURE);
            }

            break;

        default:
            puntatoreSharedMemoryTuttiUtenti[i + 1].userPid = childPid;
            puntatoreSharedMemoryTuttiUtenti[i + 1].stato = USER_OK;
            puntatoreSharedMemoryTuttiUtenti[i + 1].budget = getSoBudgetInit();
            printUtenti[i].userPid = childPid;
            printUtenti[i].stato = USER_OK;
            printUtenti[i].budget = getSoBudgetInit();
            break;
        }
    }

    /*FINE CREAZIONE UTENTI*/
    semopRisposta = semReserve(idSemaforoSincronizzazioneTuttiProcessi, -1, 0, 0);

    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
        exit(EXIT_FAILURE);
    }

    /*INIZIO Impostazione sigaction per ALARM*/
    /*no signals blocked*/
    impostaHandlerSaNoMask(&sigactionAlarmNuova, &sigactionAlarmPrecedente, SIGALRM, alarmHandler);
    impostaHandlerSaNoMask(&actNuova, &actPrecedente, SIGINT, interruptHandler);

    alarm(getSoSimSec());
    /*FINE Impostazione sigaction per ALARM*/
    /*CICLO DI VITA DEL PROCESSO MASTER*/
    while (master)
    {
        attesaNonAttiva(1000000000, 1000000000);
        /*verifico la capienza del libro mastro!*/
        if (puntatoreSharedMemoryIndiceLibroMastro[0] >= getSoRegistrySize())
        {
            raise(SIGALRM);
            motivoTerminazione = REGISTRY_FULL;
            master = MASTER_STOP;
            break;
        }
        
        /*verifico se qualcuno ha cambiato lo stato senza attendere*/
        while ((childPidWait = waitpid(-1, &childStatus, WNOHANG)) != 0 && puntatoreSharedMemoryTuttiUtenti[0].userPid > 1)
        {
            if (WIFEXITED(childStatus) == EXIT_FAILURE)
            {
                puntatoreSharedMemoryTuttiUtenti[0].userPid--;
                /*nel caso non ci siano piu' figli, oppure e' rimasto un figlio solo -- termino la simulazione*/
            }
        }

        /*verifico il numero dei processi attivi*/
        if (puntatoreSharedMemoryTuttiUtenti[0].userPid <= 1 && motivoTerminazione != ALLARME_SCATTATO)
        {
            /*EMULO L'ARRIVO DEL SEGNALE*/
            raise(SIGALRM);
            master = MASTER_STOP;
            motivoTerminazione = NO_UTENTI_VIVI;
        }
        /*verifico la mia coda di messaggi*/
        nuovoNodoCont = 0;
        if (master && msgrcv(idCodaMessaggiProcessoMaster, &m, sizeof(m.transazione) + sizeof(m.hops), 0, IPC_NOWAIT) > 0 /*&& nuovoNodoCont < 2*/)
        {
            printf("Ricevuto la transazione: %ld, %d, %d\n", m.mtype, m.transazione.receiver, m.transazione.quantita);
            /*devo creare un nuovo nodo*/
            /*verifico se ho ancora spazio per poter ospitare un nuovo nodo*/
            if (puntatoreSharedMemoryTuttiNodi[0].nodoPid < (q + getSoNodesNum()))
            {
                operazioniSemaforo.sem_flg = 0;
                operazioniSemaforo.sem_num = 0;
                operazioniSemaforo.sem_op = 2;
                semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
                if (semopRisposta != -1 && master)
                {
                    switch (childPid = fork())
                    {
                    case -1:
                        perror("fork nuovo nodo");
                        raise(SIGALRM);
                        break;
                        break;
                    case 0:
                        /*devo notificare il parent e attendere lo zero*/
                        semopRisposta = semReserve(idSemaforoSincronizzazioneTuttiProcessi, -1, 0, 0);
                        if (semopRisposta == -1)
                        {
                            perror("semop nodo");
                            exit(EXIT_FAILURE);
                        }
                        semopRisposta = semReserve(idSemaforoSincronizzazioneTuttiProcessi, 0, 0, 0);
                        if (semopRisposta == -1)
                        {
                            perror("semop nodo");
                            exit(EXIT_FAILURE);
                        }
                        sprintf(intToStrBuff, "%d", puntatoreSharedMemoryTuttiNodi[0].nodoPid - 1);
                        strcpy(parametriPerNodo[4], intToStrBuff);

                        execRisposta = execlp("./NodoBozza.out", parametriPerNodo[0], parametriPerNodo[1], parametriPerNodo[2], parametriPerNodo[3], parametriPerNodo[4], parametriPerNodo[5], parametriPerNodo[6], parametriPerNodo[7], parametriPerNodo[8], parametriPerNodo[9], parametriPerNodo[10], parametriPerNodo[11], parametriPerNodo[12], parametriPerNodo[13], parametriPerNodo[14], NULL);

                        if (execRisposta == -1)
                        {
                            perror("execlp");
                            exit(EXIT_FAILURE);
                        }

                        break;
                    default:
                        /*registro il nodo*/
                        puntatoreSharedMemoryTuttiNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1].nodoPid = childPid;
                        puntatoreSharedMemoryTuttiNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1].transazioniPendenti = 0;
                        puntatoreSharedMemoryTuttiNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1].budget = 0;
                        /*creo la coda di messaggi*/
                        msggetRisposta = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
                        if (msggetRisposta == -1)
                        {
                            perror("- msgget tutteCodeMessaggi");
                        }
                        m.hops = getSoHops();
                        m.mtype = getpid();
                        semReserve(idSemaforoAccessoCodeMessaggi, -1, puntatoreSharedMemoryTuttiNodi[0].nodoPid, 0);

                        /*CAMBIAMENTO INIZIO*/
                        if (msgsnd(msggetRisposta, &m, sizeof(m.transazione) + sizeof(m.hops), 0) != -1)
                        {
                            printf("PRIMA TRANS\n");
                        }
                        else
                        {
                            perror("ERRORE SEND");
                        }

                        /*finche' la coda del master contiene dei messaggi e finche' il nodo puo' ricevere le transazioni -> invio al nodo le transazioni*/
                        while (semReserve(idSemaforoAccessoCodeMessaggi, -1, puntatoreSharedMemoryTuttiNodi[0].nodoPid, IPC_NOWAIT) != -1 && msgrcv(idCodaMessaggiProcessoMaster, &m, sizeof(m.transazione) + sizeof(m.hops), 0, IPC_NOWAIT) > 0)
                        {
                            msgsnd(msggetRisposta, &m, sizeof(m.transazione) + sizeof(m.hops), 0);
                        }
                        semRelease(idSemaforoLimiteRisorse, SO_TP_SIZE - semctl(idSemaforoAccessoCodeMessaggi, puntatoreSharedMemoryTuttiNodi[0].nodoPid, GETVAL), 0, 0);
                        puntatoreSharedMemoryTuttiNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1].mqId = msggetRisposta;
                        printNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid].nodoPid = childPid;
                        printNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid].budget = 0;
                        break;
                    }

                    /*assegno amici al nodi appena creato*/
                    puntatoreSharedMemoryAmiciNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid * (getSoFriendsNum() * 2 + 1)] = getSoFriendsNum();
                    for (j = 1; j <= getSoFriendsNum(); j++)
                    {
                        if (indiceAmicoSuccessivo == puntatoreSharedMemoryTuttiNodi[0].nodoPid)
                        {
                            indiceAmicoSuccessivo = 1;
                        }
                        puntatoreSharedMemoryAmiciNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid * (getSoFriendsNum() * 2 + 1) + j] = indiceAmicoSuccessivo++;

                        if (indiceAmicoSuccessivo == puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1)
                        {
                            indiceAmicoSuccessivo = 1;
                        }
                    }

                    /*devo aggiungere questo nodo come amico ad altri nodi scelti casualmente*/
                    aggiungiAmico(puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1);

                    puntatoreSharedMemoryTuttiNodi[0].nodoPid += 1;

                    semopRisposta = semReserve(idSemaforoSincronizzazioneTuttiProcessi, -1, 0, 0);
                    if (semopRisposta == -1)
                    {
                        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            attesaNonAttiva(500000, 10000000);
        }
        /*stampo INFO */
        stampaTerminale(0);
    }

    /*NOTIFICO TUTTI I FIGLI*/
    if (puntatoreSharedMemoryTuttiUtenti[0].userPid >= 1)
    {
        for (cont = 0; cont < getSoUsersNum(); cont++)
        {
            if (puntatoreSharedMemoryTuttiUtenti[cont + 1].stato == USER_OK)
            {
                kill(puntatoreSharedMemoryTuttiUtenti[cont + 1].userPid, SIGUSR1);
            }
        }
    }
    for (cont = 0; cont < puntatoreSharedMemoryTuttiNodi[0].nodoPid; cont++)
    {
        kill(puntatoreSharedMemoryTuttiNodi[cont + 1].nodoPid, SIGUSR1);
    }
    /*Prima di chiudere le risorse... Attendo i processi-figli ancora in esecuzione*/
    while ((childPidWait = waitpid(-1, &childStatus, 0)) != -1)
    {
#if (ENABLE_TEST)
        printf("+ %d ha terminato con status %d\n", childPidWait, WEXITSTATUS(childStatus));
#endif
    }
    /*STAMPO CON MOTIVO DELLA TERMINAZIONE - flag = 1*/
    stampaTerminale(1);
    /*Costruisco il file di output*/
    outputLibroMastro();
    /*Chiusura delle risorse*/
    free(arrayValoriInizialiSemaforiCodeMessaggi);
    free(printNodi);
    free(printUtenti);
    eseguiDeallocamentoSemaforo(idSemaforoLimiteRisorse, 0, "- semctl idSemaforoLimiteRisorse");
    eseguiDeallocamentoCodaMessaggi(idCodaMessaggiProcessoMaster, "- msgctl idCodaMessaggiProcessoMaster");
    eseguiDetachShm(puntatoreSharedMemoryAmiciNodi, "puntatoreSharedMemoryAmiciNodi");
    eseguiDeallocamentoShm(idSharedMemoryAmiciNodi, "- shmdt idSharedMemoryAmiciNodi");
    eseguiDetachShm(puntatoreSharedMemoryTuttiUtenti, "- shmdt puntatoreSharedMemoryTuttiUtenti");
    eseguiDeallocamentoSemaforo(idSemaforoSincronizzazioneTuttiProcessi, 0, "- semctl idSemaforoSincronizzazioneTuttiProcessi");
    eseguiDeallocamentoShm(idSharedMemoryTuttiUtenti, "- shmctl idSharedMemoryTuttiUtenti");
    eseguiDeallocamentoSemaforo(idSemaforoAccessoCodeMessaggi, 0, "- semctl idSemaforoAccessoCodeMessaggi");

    for (i = 0; i < puntatoreSharedMemoryTuttiNodi[0].nodoPid; i++)
    {
        eseguiDeallocamentoCodaMessaggi(puntatoreSharedMemoryTuttiNodi[i + 1].mqId, "- msgctl puntatoreSharedMemoryTuttiNodi");
    }

    eseguiDetachShm(puntatoreSharedMemoryTuttiNodi, "- shmdt puntatoreSharedMemoryTuttiNodi");
    eseguiDeallocamentoShm(idSharedMemoryTuttiNodi, "- shmctl idSharedMemoryTuttiNodi");
    eseguiDeallocamentoSemaforo(idSemaforoAccessoIndiceLibroMastro, 0, "- semctl idSemaforoAccessoIndiceLibroMastro");
    eseguiDetachShm(puntatoreSharedMemoryIndiceLibroMastro, "puntatoreSharedMemoryIndiceLibroMastro");
    eseguiDeallocamentoShm(idSharedMemoryIndiceLibroMastro, "idSharedMemoryIndiceLibroMastro");
    eseguiDetachShm(puntatoreSharedMemoryLibroMastro, "puntatoreSharedMemoryLibroMastro");
    eseguiDeallocamentoShm(idSharedMemoryLibroMastro, "idSharedMemoryLibroMastro");
    green();
    printf("+ Risorse deallocate correttamente\n");
    reset();
    return 0;
}

/*DEFINIZIONE DELLE FUNZIONI*/
int readAllParameters(const char *configFilePath)
{
    char buffer[256];
    char *token;
    char *delimeter = "= ";
    int parsedValue;
    FILE *configFile;
    int closeResponse;
    configFile = fopen(configFilePath, "r+");
    bzero(buffer, 256);

    if (configFile != NULL)
    {
        while (fgets(buffer, 256, configFile) != NULL)
        {
            token = strtok(buffer, delimeter);
            if (strcmp(token, "SO_USERS_NUM") == 0)
            {
                setSoUsersNum(atoi(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_NODES_NUM") == 0)
            {
                setSoNodesNum(atoi(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_REWARD") == 0)
            {
                setSoReward(atoi(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_MIN_TRANS_GEN_NSEC") == 0)
            {
                setSoMinTransGenNsec(atol(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_MAX_TRANS_GEN_NSEC") == 0)
            {
                setSoMaxTransGenNsec(atol(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_RETRY") == 0)
            {
                setSoRetry(atoi(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_MIN_TRANS_PROC_NSEC") == 0)
            {
                setSoMinTransProcNsec(atol(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_MAX_TRANS_PROC_NSEC") == 0)
            {
                setSoMaxTransProcNsec(atol(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_REGISTRY_SIZE") == 0)
            {
                setSoRegistrySize(atoi(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_BUDGET_INIT") == 0)
            {
                setSoBudgetInit(atoi(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_SIM_SEC") == 0)
            {
                setSoSimSec(atoi(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_FRIENDS_NUM") == 0)
            {
                setSoFriendsNum(atoi(strtok(NULL, delimeter)));
            }
            else if (strcmp(token, "SO_HOPS") == 0)
            {
                setSoHops(atoi(strtok(NULL, delimeter)));
            }
            else
            {
                printf("Errore durante il parsing dei parametri - token %s\n", token);
                fclose(configFile);
                return -1;
            }

        }

        closeResponse = fclose(configFile);

        if (closeResponse == -1)
        {
            perror("fclose");
            return -1;
        }

#if (ENABLE_TEST)
        printf("Stampo i parametri parsati!\n");
        printf("SO_USERS_NUM=%d\n", getSoUsersNum());
        printf("SO_NODES_NUM=%d\n", getSoNodesNum());
        printf("SO_REWARD=%d\n", getSoReward());
        printf("SO_MIN_TRANS_GEN_NSEC=%ld\n", getSoMinTransGenNsec());
        printf("SO_MAX_TRANS_GEN_NSEC=%ld\n", getSoMaxTransGenNsec());
        printf("SO_RETRY=%d\n", getSoRetry());
        printf("SO_MIN_TRANS_PROC_NSEC=%ld\n", getSoMinTransProcNsec());
        printf("SO_MAX_TRANS_PROC_NSEC=%ld\n", getSoMaxTransProcNsec());
        printf("SO_REGISTRY_SIZE=%d\n", getSoRegistrySize());
        printf("SO_BUDGET_INIT=%d\n", getSoBudgetInit());
        printf("SO_SIM_SEC=%d\n", getSoSimSec());
        printf("SO_FRIENDS_NUM=%d\n", getSoFriendsNum());
        printf("SO_HOPS=%d\n", getSoHops());
#endif

        return 0;
    }
    else
    {
        perror("fopen");
        return -1;
    }
}

void alarmHandler(int sigNum)
{
    int r;
    sigset_t set, setOld;

    master = MASTER_STOP;
    motivoTerminazione = ALLARME_SCATTATO;
    /*mashero successivi ALARM*/
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    r = sigprocmask(SIG_BLOCK, &set, &setOld);
    return;
}

void interruptHandler(int sigNum)
{
    master = MASTER_STOP;
    motivoTerminazione = TERMINATO_DA_UTENTE;
}

void stampaTerminale(int flag)
{
    int sommaBudgetUtente = 0;
    int sommaBudgetNodi = 0;
    int contatoreStampa;
    int contPremat;
    int posizioneSalvata;
    char *ragione;
    char *statoUtente;

    utenteMax.budget = 0;
    utenteMin.budget = printUtenti[0].budget;
    nodoMax.budget = 0;
    nodoMin.budget = printNodi[0].budget;

    if (flag == 1)
    {
        ragione = "segnale SIGINT ricevuto\0";
        switch (motivoTerminazione)
        {
        case ALLARME_SCATTATO:
            ragione = "l'allarme scattato\0";
            break;
        case NO_UTENTI_VIVI:
            ragione = "no utenti vivi\0";
            break;
        case REGISTRY_FULL:
            ragione = "registro raggiunto capienza massima\0";
            break;
        case LIMITE_SISTEMA:
            ragione = "limite di sistema su fork raggiunto";
        default:
            break;
        }
        printf("Ragione della terminazione: \033[1;31m%s\033[0m", ragione);
    }

    contPremat = 0;
    /*stampo bilancio utenti*/

    posizioneSalvata = *(puntatoreSharedMemoryIndiceLibroMastro);

    /*riempimento array utenti*/
    for (; indiceStampaUtenti < posizioneSalvata; indiceStampaUtenti++)
    {
        for (contatoreStampa = 0; contatoreStampa < getSoUsersNum(); contatoreStampa++)
        {
            for (indicePassaCelleLibroStampa = 0; indicePassaCelleLibroStampa < getSoBlockSize() - 1; indicePassaCelleLibroStampa++)
            {
                if (puntatoreSharedMemoryLibroMastro[indiceStampaUtenti * getSoBlockSize() + indicePassaCelleLibroStampa].receiver == printUtenti[contatoreStampa].userPid)
                {
                    printUtenti[contatoreStampa].budget += puntatoreSharedMemoryLibroMastro[indiceStampaUtenti * getSoBlockSize() + indicePassaCelleLibroStampa].quantita;
                }
                if (puntatoreSharedMemoryLibroMastro[indiceStampaUtenti * getSoBlockSize() + indicePassaCelleLibroStampa].sender == printUtenti[contatoreStampa].userPid)
                {
                    printUtenti[contatoreStampa].budget -= (puntatoreSharedMemoryLibroMastro[indiceStampaUtenti * getSoBlockSize() + indicePassaCelleLibroStampa].quantita + puntatoreSharedMemoryLibroMastro[indiceStampaUtenti * getSoBlockSize() + indicePassaCelleLibroStampa].reward);
                }
            }
        }
    }

    /*rimepimento array nodi per stampa*/
    for (; indiceStampaNodi < posizioneSalvata; indiceStampaNodi++)
    {
        for (contatoreStampa = 0; contatoreStampa < puntatoreSharedMemoryTuttiNodi[0].nodoPid; contatoreStampa++)
        {
            if (puntatoreSharedMemoryLibroMastro[indiceStampaNodi * getSoBlockSize() + (getSoBlockSize() - 1)].receiver == printNodi[contatoreStampa].nodoPid)
            {
                printNodi[contatoreStampa].budget += puntatoreSharedMemoryLibroMastro[indiceStampaNodi * getSoBlockSize() + (getSoBlockSize() - 1)].quantita;
            }
        }
    }

    printf("\n\n\033[1;37m\tUTENTE[ID]|  BILANCIO | STATO\033[0m\n");
    if (getSoUsersNum() <= 20)
    {
        for (contatoreStampa = 0; contatoreStampa < getSoUsersNum(); contatoreStampa++)
        {
            if (puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].stato == USER_KO)
            {

                printUtenti[contatoreStampa].stato = USER_KO;
                contPremat++;
            }
            if (contatoreStampa % 2)
            {
                cyan();
            }
            printf("\t%09d | %09d | %s\n", printUtenti[contatoreStampa].userPid, printUtenti[contatoreStampa].budget, (printUtenti[contatoreStampa].stato == USER_OK) ? "attivo" : "term. premat.");
            if (contatoreStampa % 2)
            {
                reset();
            }
        }
    }
    else
    {
        for (contatoreStampa = 0; contatoreStampa < getSoUsersNum(); contatoreStampa++)
        {
            if (printUtenti[contatoreStampa].budget >= utenteMax.budget)
            {
                utenteMax = printUtenti[contatoreStampa];
            }
            if (printUtenti[contatoreStampa].budget <= utenteMin.budget)
            {
                utenteMin = printUtenti[contatoreStampa];
            }
            if (puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].stato == USER_KO)
            {
                printUtenti[contatoreStampa].stato = USER_KO;
                contPremat++;
            }
        }
        printf("\t%09d | %09d | %s ", utenteMax.userPid, utenteMax.budget, (utenteMax.stato == USER_OK) ? "attivo" : "term. premat.");
        blue();
        printf("<-- UTENTE con budget MAGGIORE\n");
        reset();
        printf("\t%09d | %09d | %s ", utenteMin.userPid, utenteMin.budget, (utenteMin.stato == USER_OK) ? "attivo" : "term. premat.");
        red();
        printf("<-- UTENTE con budget MINORE\n");
        reset();
    }
    if (flag == 1)
    {
        printf("\n ******************************************\n# Utenti terminati prematuramente: %d / %d #\n ******************************************\n", contPremat, getSoUsersNum());
        master = MASTER_STOP;
    }

    printf("\n\033[1;37m\t # | NODO[PID] |  BILANCIO | TRANS. P.\033[0m\n");
    if (puntatoreSharedMemoryTuttiNodi[0].nodoPid <= 20)
    {
        for (contatoreStampa = 0; contatoreStampa < puntatoreSharedMemoryTuttiNodi[0].nodoPid; contatoreStampa++)
        {
            if (contatoreStampa % 2)
            {
                yellow();
            }
            printf("\t%02d | %09d | %09d | %09d\n", (contatoreStampa + 1), printNodi[contatoreStampa].nodoPid, printNodi[contatoreStampa].budget, puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].transazioniPendenti);
            if (contatoreStampa % 2)
            {
                reset();
            }
        }
    }
    else
    {
        for (contatoreStampa = 0; contatoreStampa < puntatoreSharedMemoryTuttiNodi[0].nodoPid; contatoreStampa++)
        {
            if (printNodi[contatoreStampa].budget >= nodoMax.budget)
            {
                nodoMax = printNodi[contatoreStampa];
                nodoMax.transazioniPendenti = puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].transazioniPendenti;
            }
            if (printNodi[contatoreStampa].budget <= nodoMin.budget)
            {
                nodoMin = printNodi[contatoreStampa];
                nodoMin.transazioniPendenti = puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].transazioniPendenti;
            }
        }
        
        printf("\t 1 | %09d | %09d | %09d", nodoMax.nodoPid, nodoMax.budget, nodoMax.transazioniPendenti);
        blue();
        printf(" <-- NODO con budget MAGGIORE\n");
        reset();
        printf("\t 0 | %09d | %09d | %09d", nodoMin.nodoPid, nodoMin.budget, nodoMin.transazioniPendenti);
        red();
        printf(" <-- NODO con budget MINORE\n");
        reset();
        printf("\nTOTALE NODI: %d\n", puntatoreSharedMemoryTuttiNodi[0].nodoPid);
    }
    printf("\n *************************\n# Numero di blocchi: \033[1;37m%d\033[0m #\n *************************\n", *(puntatoreSharedMemoryIndiceLibroMastro));
}

void outputLibroMastro()
{
    FILE *out;
    char buffer[128];
    int index;
    int blocco;
    out = fopen("./registro.dat", "w+");
    if (out == NULL)
    {
        perror("fopen: outputLibroMastro");
        exit(EXIT_FAILURE);
    }
    bzero(buffer, 128);

    for (index = 0; index < *puntatoreSharedMemoryIndiceLibroMastro; index++)
    {
        sprintf(buffer, "| RECEIVER(PID)| SENDER(PID)  | QUANTITA     | TIMESTAMP\n");
        fputs(buffer, out);
        for (blocco = 0; blocco < getSoBlockSize(); blocco++)
        {
            sprintf(buffer, "| %013d| %013d| %013d| %s", puntatoreSharedMemoryLibroMastro[getSoBlockSize() * index + blocco].receiver, puntatoreSharedMemoryLibroMastro[getSoBlockSize() * index + blocco].sender, puntatoreSharedMemoryLibroMastro[getSoBlockSize() * index + blocco].quantita, ctime(&puntatoreSharedMemoryLibroMastro[getSoBlockSize() * index + blocco].timestamp.tv_sec));
            fputs(buffer, out);
        }
        fputs("\n", out);
    }
    sprintf(buffer, "Numero blocchi: %d", puntatoreSharedMemoryIndiceLibroMastro[0]);
    fputs(buffer, out);

    if (fclose(out) != 0)
    {
        perror("fclose: outputLibroMastro");
        exit(EXIT_FAILURE);
    }

    cyan();
    printf("+ Output file: ./registro.dat\n");
    reset();

    return;
}

void aggiungiAmico(int numeroOrdine)
{
    /*contatore per SO_FRIENDS_NUM*/
    int c;
    /*contatore per posti disponibi per utente*/
    int ci;
    /*ciclo per SO_FRINEDS_NUM volte*/
    for (c = 0; c < getSoFriendsNum(); c++)
    {
        /*se nodo chelto non acetta piu' amici lo ignoro*/
        if (puntatoreSharedMemoryAmiciNodi[nextFriend * (2 * getSoFriendsNum() + 1)] == 2 * getSoFriendsNum())
        {
            #if(ENABLE_TEST)
            printf("Nodo %d non accetta piu' amici!\n", puntatoreSharedMemoryTuttiNodi[nextFriend + 1].nodoPid);
            #endif
        }
        else
        {
            /*altrimenti cerco la prima cella utile nel suo pool di amici e aggiungo un nodo*/
            for (ci = 1; ci <= 2 * getSoFriendsNum(); ci++)
            {
                if (puntatoreSharedMemoryAmiciNodi[nextFriend * (2 * getSoFriendsNum() + 1) + ci] == -1)
                {
                    puntatoreSharedMemoryAmiciNodi[nextFriend * (2 * getSoFriendsNum() + 1) + ci] = numeroOrdine;
                    puntatoreSharedMemoryAmiciNodi[nextFriend * (2 * getSoFriendsNum() + 1)] += 1; /*incremento di 1 il numero degli amici*/
                    nextFriend++;
                    if ((nextFriend + 1) == puntatoreSharedMemoryTuttiNodi[0].nodoPid)
                    {
                        nextFriend = 0;
                    }
                    /*non appena mi registro - esco*/
                    break;
                }
            }
        }
    }
}