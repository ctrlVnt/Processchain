#include "process_chain.h"

/********************************/
/*******PARAMETRI**GLOBALI*******/
/********************************/
/*ID SM*/
/*ID della SM contenente il libro mastro*/
int idSharedMemoryLibroMastro;
/*ID SM dove si trova l'indice del libro mastro*/
int idSharedMemoryIndiceLibroMastro;
/*ID della SM degli'ID delle CODE di messaggi alla quale devo fare l'attach*/
int idSharedMemoryTuttiNodi;
/*ID della SM contenente tutti i PID dei processi utenti - visti come destinatari*/
int idSharedMemoryTuttiUtenti;
/*ID della SM che contiene i NODE_FRINEDS di ciascun nodo*/
int idSharedMemoryAmiciNodi;
/*******/

/*Puntatori SM*/
/*Dopo l'attach, punta alla porzione di memoria dove si trova effettivamente il libro mastro*/
transazione *puntatoreSharedMemoryLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria condivisa dove si trova effettivamente l'indice del libro mastro*/
int *puntatoreSharedMemoryIndiceLibroMastro;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente gli id di tutte le code di messaggi, attenzione primo campo e' HEADER*/
nodo *puntatoreSharedMemoryTuttiNodi;
/*Dopo l'attach, punta alla porzione di memoria dove si trovano effettivamente i PID degli utenti*/
utente *puntatoreSharedMemoryTuttiUtenti;
/*Dopo l'attach punta alla porzione di memroia dove si trovano gli amici del nodo*/
int *puntatoreSharedMemoryAmiciNodi;
/*************/

/*Semafori*/
/*
Id del semaforo che regola l'accesso all'indice
Il semaforo di tipo binario.
*/
int idSemaforoAccessoIndiceLibroMastro;
/*
Id del semaforo che regola l'accesso alla MQ associata al nodo
Il valore iniziale e' pari alla capacita' della transaction pool.
*/
int idSemaforoAccessoCodeMessaggi;
/*Id del semaforo per sincronizzare l'avvio dei processi*/
int idSemaforoSincronizzazioneTuttiProcessi;
/**********/

/*variabile struttura per effettuare le operazioni sul semaforo*/
/*streuttura generica per dialogare con semafori IPC V*/
struct sembuf operazioniSemaforo;
/*array contenente i valori iniziali di ciascun semaforo associato ad ogni MQ*/
unsigned short *arrayValoriInizialiSemaforiCodeMessaggi;
/*struttura utilizzata nella semctl*/
union semun semaforoUnion;
/***************************************************************/

/*variabili strutture per gestire i segnali*/
/*sigaction da settare per gestire ALARM - timer della simulazione*/
struct sigaction sigactionAlarmNuova;
/*sigaction dove memorizzare lo stato precedente*/
struct sigaction sigactionAlarmPrecedente;
/*maschera per SIGALARM*/
sigset_t maskSetForAlarm;
/*sigaction generica - uso generale*/
struct sigaction act;
/*old sigaction generica - uso generale*/
struct sigaction actPrecedente;
/*******************************************/
/*Coda di messaggi del master*/
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
int master;
int numeroNodi;
int childPid;
int childPidWait;
int childStatus;
int contatoreUtentiVivi;
int motivoTerminazione;
int idCodaMessaggiProcessoMaster;
/*********************/

/*Variabili necessari per poter avviare i nodi, successivamente gli utenti*/
char parametriPerNodo[14][32];
char intToStrBuff[32];
char parametriPerUtente[16][32];
/**************************************************************************/

int q;

/*******************/
/*FUNZIONI*/
/*Inizializza le variabili globali da un file di input TXT. */
int readAllParameters();
/*gestione del segnale*/
void alarmHandler(int sigNum);
/*stampa terminale*/
void stampaTerminale();
/*stampa il libro mastro in file di testo*/
void outputLibroMastro();
/*la funzione che aggiunge amici*/
void aggiungiAmico(int newPid, int numeroOrdine);
/**********/

/*VARIABILI PER LA STAMPA*/
utente utenteMax;
utente utenteMin;
nodo nodoMax;
nodo nodoMin;
/********/

int main(int argc, char const *argv[])
{
    q = 10;
    printf("Sono MASTER[%d]\n", getpid());
    /*Parsing dei parametri a RUN-TIME*/
    readAllParametersRisposta = readAllParameters();
    if (readAllParametersRisposta == -1)
    {
        perror("- fetch parameters");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ Parsing parametri avvenuto correttamente\n");
#endif
    contatoreUtentiVivi = getSoUsersNum();
    nextFriend = 0;

    /*Inizializzazione LIBRO MASTRO*/

    /*SM*/
    idSharedMemoryLibroMastro = shmget(IPC_PRIVATE, getSoRegistrySize() * SO_BLOCK_SIZE * sizeof(transazione), 0600 | IPC_CREAT);
    if (idSharedMemoryLibroMastro == -1)
    {
        perror("- shmget idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSharedMemoryLibroMastro creato con successo - %d\n", idSharedMemoryLibroMastro);
#endif
    puntatoreSharedMemoryLibroMastro = (transazione *)shmat(idSharedMemoryLibroMastro, NULL, 0);

    if (errno == 22)
    {
        perror("- shmat idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ puntatore al libroMastro creato con successo\n");
#endif

    /*Inizializzazione INDICE LIBRO MASTRO*/
    /*SM*/
    idSharedMemoryIndiceLibroMastro = shmget(IPC_PRIVATE, sizeof(int), 0600 | IPC_CREAT);
    if (idSharedMemoryIndiceLibroMastro == -1)
    {
        perror("- shmget idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TETS)
    printf("+ idSharedMemoryIndiceLibroMastro creato con successo - %d\n", idSharedMemoryIndiceLibroMastro);
#endif
    puntatoreSharedMemoryIndiceLibroMastro = (int *)shmat(idSharedMemoryIndiceLibroMastro, NULL, 0);
    if (*puntatoreSharedMemoryIndiceLibroMastro == -1)
    {
        perror("- shmat idSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    puntatoreSharedMemoryIndiceLibroMastro[0] = 0;
#if (ENABLE_TEST)
    printf("+ indice creato con successo e inizializzato a %d\n", puntatoreSharedMemoryIndiceLibroMastro[0]);
#endif

    /*SEMAFORO*/
    idSemaforoAccessoIndiceLibroMastro = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
    if (idSemaforoAccessoIndiceLibroMastro == -1)
    {
        perror("- semget idSemaforoAccessoIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSemaforoAccessoIndiceLibroMastro creato con successo - %d\n", idSemaforoAccessoIndiceLibroMastro);
#endif
    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_num = 0;
    operazioniSemaforo.sem_op = 1;
    semopRisposta = semop(idSemaforoAccessoIndiceLibroMastro, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoAccessoIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ semaforo idSemaforoAccessoIndiceLibroMastro inizialiizato a 1\n");
#endif

    /*FINE Inizializzazione INDICE LIBRO MASTRO*/

    /*SM*/
    /*REMINDER x2, prima cella di questa SM indica il NUMERO totale di CODE presenti, quindi rispecchia anche il numero dei nodi presenti*/
    idSharedMemoryTuttiNodi = shmget(IPC_PRIVATE, sizeof(nodo) * (q * getSoNodesNum() + 1), 0600 | IPC_CREAT);
    if (idSharedMemoryTuttiNodi == -1)
    {
        perror("shmget idSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSharedMemoryTuttiNodi creato con successo - %d\n", idSharedMemoryTuttiNodi);
#endif
    puntatoreSharedMemoryTuttiNodi = (nodo *)shmat(idSharedMemoryTuttiNodi, NULL, 0);
    if (errno == EINVAL)
    {
        perror("- shmat idSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }

    /*creo code di messaggi per processo Master*/
    idCodaMessaggiProcessoMaster = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
    if (idCodaMessaggiProcessoMaster == -1)
    {
        perror("- msgget tutteCodeMessaggi");
        exit(EXIT_FAILURE);
    }
    /*continee il numero totale di nodi*/
    puntatoreSharedMemoryTuttiNodi[0].nodoPid = getSoNodesNum();
    puntatoreSharedMemoryTuttiNodi[0].budget = -1;
    puntatoreSharedMemoryTuttiNodi[0].mqId = msggetRisposta; /*REMINDER: coda di messaggi master*/
    puntatoreSharedMemoryTuttiNodi[0].transazioniPendenti = -1;
#if (ENABLE_TEST)
    printf("+ puntatoreSharedMemoryTuttiNodi creato con successo, numero totale di MQ - %d\n", puntatoreSharedMemoryTuttiNodi[0].nodoPid);
#endif
    numeroNodi = 0;
    for (numeroNodi; numeroNodi < getSoNodesNum(); numeroNodi++)
    {
        msggetRisposta = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
        if (msggetRisposta == -1)
        {
            perror("- msgget tutteCodeMessaggi");
            exit(EXIT_FAILURE);
        }
        puntatoreSharedMemoryTuttiNodi[numeroNodi + 1].mqId = msggetRisposta;
        puntatoreSharedMemoryTuttiNodi[numeroNodi + 1].transazioniPendenti = 0;
        //puntatoreSharedMemoryTuttiNodi[numeroNodi + 1].friends = NULL;
#if (ENABLE_TEST)
        printf("+ mssget registrata all'indice %d con id - %d\n", (numeroNodi + 1), msggetRisposta);
#endif
    }
#if (ENABLE_TEST)
    printf("+ registrazione tutteCodeMessaggi avvenuta con successo, totale code %d\n", numeroNodi);
#endif
    /*suppongo di non poter avere piu' di 2*SO_FRIENDS_NUM +1 campo header amici*/
    idSharedMemoryAmiciNodi = shmget(IPC_PRIVATE, q *  getSoNodesNum() * (getSoFriendsNum()*2 + 1) * sizeof(int), 0600 | IPC_CREAT);
    if (idSharedMemoryAmiciNodi == -1)
    {
        perror("shmget idSharedMemoryAmiciNodi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSharedMemoryAmiciNodi creato con successo - %d\n", idSharedMemoryAmiciNodi);
#endif
    puntatoreSharedMemoryAmiciNodi = (int *)shmat(idSharedMemoryAmiciNodi, NULL, 0);
    if (errno == EINVAL)
    {
        perror("- shmat idSharedMemoryAmiciNodi");
        exit(EXIT_FAILURE);
    }
    /*setto ogni cella della shared memory a -1*/
    memset(puntatoreSharedMemoryAmiciNodi, -1, q * getSoNodesNum() * (getSoFriendsNum()*2 + 1) * sizeof(int));
    /*SEMAFORI*/
    idSemaforoAccessoCodeMessaggi = semget(IPC_PRIVATE, q * getSoNodesNum(), 0600 | IPC_CREAT);
    if (idSemaforoAccessoCodeMessaggi == -1)
    {
        perror("- semget idSemaforoAccessoCodeMessaggi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSemaforoAccessoCodeMessaggi inizializzato correttamente con id - %d\n", idSemaforoAccessoCodeMessaggi);
#endif

    arrayValoriInizialiSemaforiCodeMessaggi = (unsigned short *)calloc(getSoNodesNum() * q, sizeof(unsigned short));
    if (arrayValoriInizialiSemaforiCodeMessaggi == NULL)
    {
        perror("- calloc arrayValoriInizialiSemaforiCodeMessaggi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ arrayValoriInizialiSemaforiCodeMessaggi inizializzato correttamente\n");
#endif
    i = 0;
    for (i; i < getSoNodesNum() * q; i++)
    {
        arrayValoriInizialiSemaforiCodeMessaggi[i] = getTpSize();
    }
#if (ENABLE_TEST)
    printf("+ arrayValoriInizialiSemaforiCodeMessaggi popolato correttamente\n");
#endif
    semaforoUnion.array = arrayValoriInizialiSemaforiCodeMessaggi;
    semctlRisposta = semctl(idSemaforoAccessoCodeMessaggi, 0, SETALL, semaforoUnion);
    if (semctlRisposta == -1)
    {
        perror("- semctl SETALL");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ set semafori idSemaforoAccessoCodeMessaggi inizializzato correttamente\n");
#endif

    /*FINE Inizializzazione SM delle MQ*/
    /*INIZIO Inizializzazione SM che contiene i PID degli USER*/
    idSharedMemoryTuttiUtenti = shmget(IPC_PRIVATE, (getSoUsersNum() + 1) * sizeof(utente), 0600 | IPC_CREAT);
    if (idSharedMemoryTuttiUtenti == -1)
    {
        perror("- shmget idSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSharedMemoryTuttiUtenti creato con successo - %d\n", idSharedMemoryTuttiUtenti);
#endif

    /*FINE Inizializzazione SM che contiene i PID degli USER*/

    /*INIZIO Semaforo di sincronizzazione*/

    idSemaforoSincronizzazioneTuttiProcessi = semget(IPC_PRIVATE, 1, 0600 | IPC_CREAT);
    if (idSemaforoSincronizzazioneTuttiProcessi == -1)
    {
        perror("- semget idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ idSemaforoSincronizzazioneTuttiProcessi inizializzato correttamente con id - %d\n", idSemaforoSincronizzazioneTuttiProcessi);
#endif
    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_op = getSoNodesNum() + 1;
    operazioniSemaforo.sem_num = 0;
    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ inizio sincronizzare NODI\n");
#endif

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
    sprintf(intToStrBuff, "%d", idSemaforoAccessoIndiceLibroMastro); /*sostituire con SO_HOPS o SO_NUM_FRIEND*/
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
    /*******************************************/

    i = 0;
    for (i; i < getSoNodesNum(); i++)
    {
        switch ((childPid = fork()))
        {
        case -1:
            perror("fork");
            exit(EXIT_FAILURE);
            break;
        case 0:
            /*QUA HO EREDITATO TUTTI I PUNTATORI*/
            /*devo notificare il parent e attendere lo zero*/
            operazioniSemaforo.sem_flg = 0;
            operazioniSemaforo.sem_num = 0;
            operazioniSemaforo.sem_op = -1;
            semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }
            operazioniSemaforo.sem_flg = 0;
            operazioniSemaforo.sem_num = 0;
            operazioniSemaforo.sem_op = 0;
            semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }

            /***********************************************/
            /*INIZIO Costruire la lista di parametri*/
            sprintf(intToStrBuff, "%d", /*i-esimo nodo*/ i);
            strcpy(parametriPerNodo[4], intToStrBuff);
            /*FINE Lista*/

            // printf("+ Tentativo eseguire la execlp\n");
            /*PUNTO FORTE TROVATO - non c'e' da gestire l'array NULL terminated*/
            execRisposta = execlp("./NodoBozza.out", parametriPerNodo[0], parametriPerNodo[1], parametriPerNodo[2], parametriPerNodo[3], parametriPerNodo[4], parametriPerNodo[5], parametriPerNodo[6], parametriPerNodo[7], parametriPerNodo[8], parametriPerNodo[9], parametriPerNodo[10], parametriPerNodo[11], parametriPerNodo[12], parametriPerNodo[13], NULL);
            if (execRisposta == -1)
            {
                perror("execlp");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            /*NON MI RICORDO SE DEVO FARE QUALCOSA QUA*/
            /*eh invece si'*/
            puntatoreSharedMemoryTuttiNodi[i + 1].budget = 0;
            puntatoreSharedMemoryTuttiNodi[i + 1].nodoPid = childPid;
            break;
        }
    }

    /*copiare da file su GH*/
    /*assegno gli amici ai carissimi nodi*/
    int indiceAmicoSuccessivo = 2;
    for (i = 0; i < getSoNodesNum(); i++)
    {
#if (ENABLE_TEST)
        printf("Nodo [%d] con amici:\n", puntatoreSharedMemoryTuttiNodi[nextFriend].nodoPid);
#endif
        puntatoreSharedMemoryAmiciNodi[i * (getSoFriendsNum() * 2 + 1)] = getSoFriendsNum();
        indiceAmicoSuccessivo = i + 2;
        if(indiceAmicoSuccessivo == getSoNodesNum() + 1)
        {
            indiceAmicoSuccessivo = 1;
        }
        for (j = 1; j <= getSoFriendsNum(); j++)
        {
            puntatoreSharedMemoryAmiciNodi[i * (getSoFriendsNum() * 2 + 1) + j] = (indiceAmicoSuccessivo++);
#if (ENABLE_TEST)
            printf("[%d]", puntatoreSharedMemoryAmiciNodi[(i - 1) * getSoFriendsNum() + j]);
#endif
            if(indiceAmicoSuccessivo == getSoNodesNum() + 1)
            {
                indiceAmicoSuccessivo = 1;
            }
        }
#if (ENABLE_TEST)
        printf("\n");
#endif
    }

    /*AMICI ASSEGNATI*/
    for(int k = 0; k < getSoNodesNum(); k++)
    {
        printf("NODO %d : \t%d\t|", (k + 1), puntatoreSharedMemoryAmiciNodi[k*(getSoFriendsNum()*2 + 1)]);
        for(int g = 1; g <= getSoFriendsNum()*2; g++)
        {
            printf("%d\t", puntatoreSharedMemoryAmiciNodi[(k*(getSoFriendsNum()*2 + 1)) + g]);
        }
        printf("\n");
    }

    // sleep(100);

#if (ENABLE_TEST)
    printf("popolata porzione di memoria contenente amici nodi\n");
#endif

    /*FINE CREAZIONE NODI*/

    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_op = -1;
    operazioniSemaforo.sem_num = 0;
    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ nodi sono notificati correttamente\n");
#endif

    /*FINE Semaforo di sincronizzazione*/

    /*PARTE DEGLI UTENTI*/
    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_op = getSoUsersNum() + 1;
    operazioniSemaforo.sem_num = 0;
    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ inizio sincronizzare UTENTI\n");
#endif

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
    /********************************************************/
    int indiceNodi = 0;
    for (indiceNodi; indiceNodi < getSoUsersNum(); indiceNodi++)
    {
        switch (childPid = fork())
        {
        case -1:
            perror("fork");
            exit(EXIT_FAILURE);
            break;
        case 0:
            /*QUA HO EREDITATO TUTTI I PUNTATORI*/
            /*devo notificare il parent e attendere lo zero*/

            operazioniSemaforo.sem_flg = 0;
            operazioniSemaforo.sem_num = 0;
            operazioniSemaforo.sem_op = -1;
            semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }
            operazioniSemaforo.sem_flg = 0;
            operazioniSemaforo.sem_num = 0;
            operazioniSemaforo.sem_op = 0;
            semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
            if (semopRisposta == -1)
            {
                perror("semop nodo");
                exit(EXIT_FAILURE);
            }

            /**********************************************/
            /* UNICO PARAMETRO VARIABILE E' l'ID del nodo */
            /**********************************************/
            sprintf(intToStrBuff, "%d", /*i-esimo nodo*/ indiceNodi);
            strcpy(parametriPerUtente[4], intToStrBuff);
            /**********************************************/

            //printf("+ Tentativo eseguire la execlp\n");
            /*PUNTO FORTE TROVATO - non c'e' da gestire l'array NULL terminated*/
            execRisposta = execlp("./UtenteBozza.out", parametriPerUtente[0], parametriPerUtente[1], parametriPerUtente[2], parametriPerUtente[3], parametriPerUtente[4], parametriPerUtente[5], parametriPerUtente[6], parametriPerUtente[7], parametriPerUtente[8], parametriPerUtente[9], parametriPerUtente[10], parametriPerUtente[11], parametriPerUtente[12], parametriPerUtente[13], parametriPerUtente[14], parametriPerUtente[15], NULL);
            if (execRisposta == -1)
            {
                perror("execlp");
                exit(EXIT_FAILURE);
            }

            break;
        default:
            puntatoreSharedMemoryTuttiUtenti[indiceNodi + 1].userPid = childPid;
            puntatoreSharedMemoryTuttiUtenti[indiceNodi + 1].stato = USER_OK;
            puntatoreSharedMemoryTuttiUtenti[indiceNodi + 1].budget = getSoBudgetInit();
            /**/
#if (ENABLE_TEST)
            // printf("+ %d UTENTE[%d] registrato correttamente\n", i, childPid);
#endif
            // sleep(1);
            break;
        }
    }

    /*FINE CREAZIONE UTENTI*/

    operazioniSemaforo.sem_flg = 0;
    operazioniSemaforo.sem_op = -1;
    operazioniSemaforo.sem_num = 0;
    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
    if (semopRisposta == -1)
    {
        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
        exit(EXIT_FAILURE);
    }
#if (ENABLE_TEST)
    printf("+ utenti sono notificati correttamente\n");
#endif

    /*INIZIO Impostazione sigaction per ALARM*/
    /*no signals blocked*/
    sigemptyset(&sigactionAlarmNuova.sa_mask);
    sigactionAlarmNuova.sa_flags = 0;
    sigactionAlarmNuova.sa_handler = alarmHandler;
    sigaction(SIGALRM, &sigactionAlarmNuova, &sigactionAlarmPrecedente);
#if (ENABLE_TEST)
    printf("+ sigaction per ALARM impostato con successo");
#endif
    alarm(getSoSimSec());
#if (ENABLE_TEST)
    printf("+ timer avviato: %d sec.\n", getSoSimSec());
#endif
    /*FINE Impostazione sigaction per ALARM*/

    /*CICLO DI VITA DEL PROCESSO MASTER*/

    master = MASTER_CONTINUE;
    motivoTerminazione = -1;
    while (master)
    {
        /*TODO*/
        sleep(1);
        /*verifico la capienza del libro mastro!*/
        if (puntatoreSharedMemoryIndiceLibroMastro[0] == getSoRegistrySize())
        {
            raise(SIGALRM);
            motivoTerminazione = REGISTRY_FULL;
            stampaTerminale(1);
            master = MASTER_STOP;
            break; /*perhe' non voglio che venga eseguito codice sottostante, alterniativa spostare questo pezzo alla fine*/
        }
        /*stampo INFO */
        stampaTerminale(0);
        /**/
        /*verifico se qualcuno ha cambiato lo stato senza attendere*/
        int a = 0;
        while ((childPidWait = waitpid(-1, &childStatus, WNOHANG)) != 0 && puntatoreSharedMemoryTuttiUtenti[0].userPid > 1)
        {
            if (WIFEXITED(childStatus))
            {
                printf("- %d ha terminato prematuramente\n", childPidWait);
                puntatoreSharedMemoryTuttiUtenti[0].userPid--;
                /*nel caso non ci siano piu' figli, oppure e' rimasto un figlio solo -- termino la simulazione*/
                /* }*/
            }
            printf("aiaaa\n");
            // printf("qua\n");
        }
        /*verifico il numero dei processi attivi*/
        if (puntatoreSharedMemoryTuttiUtenti[0].userPid <= 1 && motivoTerminazione != ALLARME_SCATTATO)
        {
            /*COME SE ALLARME SCATASSE*/
            raise(SIGALRM);
            master = MASTER_STOP;
            motivoTerminazione = NO_UTENTI_VIVI;
        }
        /*verifico la mia coda di messaggi*/
        while(master && msgrcv(idCodaMessaggiProcessoMaster, &m, sizeof(m.transazione) + sizeof(m.hops), 0, IPC_NOWAIT) > 0)
        {
            printf("Ricevuto la transazione: %ld, %d, %d\n", m.mtype, m.transazione.receiver, m.transazione.quantita);
            /*devo crearo un nuovo nodo*/
            /*verifico se ho ancora spazio per poter ospitare un nuovo nodo*/
            if(puntatoreSharedMemoryTuttiNodi[0].nodoPid < (q * getSoNodesNum()))
            {
                operazioniSemaforo.sem_flg = 0;
                operazioniSemaforo.sem_num = 0;
                operazioniSemaforo.sem_op = 2;
                semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
                if(semopRisposta != -1 && master)
                {
                    switch(childPid = fork())
                    {
                        case -1:
                            perror("fork nuovo nodo");
                            raise(SIGALRM);
                            break;
                        break;
                        case 0:
                            /*devo notificare il parent e attendere lo zero*/
                            operazioniSemaforo.sem_flg = 0;
                            operazioniSemaforo.sem_num = 0;
                            operazioniSemaforo.sem_op = -1;
                            semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
                            if (semopRisposta == -1)
                            {
                                perror("semop nodo");
                                exit(EXIT_FAILURE);
                            }
                            operazioniSemaforo.sem_flg = 0;
                            operazioniSemaforo.sem_num = 0;
                            operazioniSemaforo.sem_op = 0;
                            semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
                            if (semopRisposta == -1)
                            {
                                perror("semop nodo");
                                exit(EXIT_FAILURE);
                            }
                            sprintf(intToStrBuff, "%d", /*i-esimo nodo*/ puntatoreSharedMemoryTuttiNodi[0].nodoPid - 1);
                            strcpy(parametriPerNodo[4], intToStrBuff);

                            execRisposta = execlp("./NodoBozza.out", parametriPerNodo[0], parametriPerNodo[1], parametriPerNodo[2], parametriPerNodo[3], parametriPerNodo[4], parametriPerNodo[5], parametriPerNodo[6], parametriPerNodo[7], parametriPerNodo[8], parametriPerNodo[9], parametriPerNodo[10], parametriPerNodo[11], parametriPerNodo[12], parametriPerNodo[13], NULL);
                            if (execRisposta == -1)
                            {
                                perror("execlp");
                                exit(EXIT_FAILURE);
                            }

                            break;
                        default:
                            /*registro il nodo*/
                            puntatoreSharedMemoryTuttiNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1].nodoPid = childPid;
                            puntatoreSharedMemoryTuttiNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1].transazioniPendenti = 1;
                            puntatoreSharedMemoryTuttiNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1].budget = 0;
                            /*creo la coda di messaggi*/
                            msggetRisposta = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
                            if (msggetRisposta == -1)
                            {
                                perror("- msgget tutteCodeMessaggi");
                            }
                            m.hops = getSoHops();
                            m.mtype = m.transazione.sender;
                            if(msgsnd(msggetRisposta, &m, sizeof(m.transazione) + sizeof(m.hops), 0) > 0)
                            {
                                printf("PRIMA TRANS\n");
                            }
                            else
                            {
                                perror("ERRORE SEND");
                            }
                            puntatoreSharedMemoryTuttiNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1].mqId =  msggetRisposta;
                            break;
                    }

                    /*assegno amici al nodi appena creato*/
                    puntatoreSharedMemoryAmiciNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid * (getSoFriendsNum() * 2 + 1)] = getSoFriendsNum();
                    for (j = 1; j <= getSoFriendsNum(); j++)
                    {
                        if(indiceAmicoSuccessivo == getSoNodesNum() + 1)
                        {
                            indiceAmicoSuccessivo = 1;
                        }
                        puntatoreSharedMemoryAmiciNodi[puntatoreSharedMemoryTuttiNodi[0].nodoPid * (getSoFriendsNum() * 2 + 1) + j] = indiceAmicoSuccessivo++;
            #if (ENABLE_TEST)
                        printf("[%d]", puntatoreSharedMemoryAmiciNodi[(puntatoreSharedMemoryTuttiNodi[0].nodoPid - 1) * (getSoFriendsNum() * 2 + 1) + j]);
            #endif
                        if(indiceAmicoSuccessivo == puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1)
                        {
                            indiceAmicoSuccessivo = 1;
                        }
                    }

                    /*devo aggiungere questo nodo come amico ad altri nodi scelti casualmente*/
                    aggiungiAmico(childPid, puntatoreSharedMemoryTuttiNodi[0].nodoPid + 1);

                    operazioniSemaforo.sem_flg = 0;
                    operazioniSemaforo.sem_op = -1;
                    operazioniSemaforo.sem_num = 0;
                    
                    /*incremento il numero totale di nodi*/
                    puntatoreSharedMemoryTuttiNodi[0].nodoPid += 1;

                    semopRisposta = semop(idSemaforoSincronizzazioneTuttiProcessi, &operazioniSemaforo, 1);
                    if (semopRisposta == -1)
                    {
                        perror("- semop idSemaforoSincronizzazioneTuttiProcessi dopo NODI");
                        exit(EXIT_FAILURE);
                    }
                    printf("FORK di un nuovo nodo %d avvenuta con successo!\n", childPid);
                    for(int k = 0; k < puntatoreSharedMemoryTuttiNodi[0].nodoPid; k++)
                    {
                        printf("NODO %d : \t%d\t|", (k + 1), puntatoreSharedMemoryAmiciNodi[k*(getSoFriendsNum()*2 + 1)]);
                        for(int g = 1; g <= getSoFriendsNum()*2; g++)
                        {
                            printf("%d\t", puntatoreSharedMemoryAmiciNodi[(k*(getSoFriendsNum()*2 + 1)) + g]);
                        }
                        printf("\n");
                    }
                }
            }
            else
            /*purtroppo ho esaurito le mie risorse*/
            {
                /*notifico il sender e li restituisco i soldi*/
                for(int x = 1; x <= puntatoreSharedMemoryTuttiUtenti[0].userPid; x++)
                {
                    if(m.mtype == puntatoreSharedMemoryTuttiUtenti[x].userPid)
                    {
                        puntatoreSharedMemoryTuttiUtenti[x].budget = m.transazione.quantita;
                        break;/*continuo il ciclo di vita del master*/
                    }
                }
            }
        }
    }

    /***********************************/
    /*Prima di chiudere le risorse... Attendo i processi-figli ancora in esecuzione*/
    while ((childPidWait = waitpid(-1, &childStatus, 0)) != -1)
    {
        printf("+ %d ha terminato con status %d\n", childPidWait, WEXITSTATUS(childStatus));
    }
    /********************************************************/
    /*STAMPO CON MOTIVO DELLA TERMINAZIONE - flag = 1*/
    stampaTerminale(1);
    /**********************************/
    /*Costruisco il file di output*/
    outputLibroMastro();

    /*Chiusura delle risorse*/
    free(arrayValoriInizialiSemaforiCodeMessaggi);
    msgctlRisposta = msgctl(idCodaMessaggiProcessoMaster, IPC_RMID, NULL);
    if (msgctlRisposta == -1)
    {
        perror("- msgctl"); /*essere piu' dettagliato*/
        exit(EXIT_FAILURE);
    }
    eseguiDetachShm(puntatoreSharedMemoryAmiciNodi, "puntatoreSharedMemoryAmiciNodi");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryAmiciNodi);
    if (shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryAmiciNodi");
        exit(EXIT_FAILURE);
    }*/
    eseguiDeallocamentoShm(idSharedMemoryAmiciNodi, "idSharedMemoryAmiciNodi");
    /*shmctlRisposta = shmctl(idSharedMemoryAmiciNodi, IPC_RMID, NULL);
    if (shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryAmiciNodi");
        exit(EXIT_FAILURE);
    }*/
    eseguiDetachShm(puntatoreSharedMemoryTuttiUtenti,"idSharedMemoryAmiciNodi");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiUtenti);
    if (shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }*/
    semctlRisposta = semctl(idSemaforoSincronizzazioneTuttiProcessi, 0, IPC_RMID);
    if (semctlRisposta == -1)
    {
        perror("- semctl idSemaforoSincronizzazioneTuttiProcessi");
        exit(EXIT_FAILURE);
    }
    eseguiDeallocamentoShm(idSharedMemoryTuttiUtenti, "idSharedMemoryTuttiUtenti");
    /*shmctlRisposta = shmctl(idSharedMemoryTuttiUtenti, IPC_RMID, NULL);
    if (shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryTuttiUtenti");
        exit(EXIT_FAILURE);
    }*/
    semctlRisposta = semctl(idSemaforoAccessoCodeMessaggi, 0, IPC_RMID);
    if (semctlRisposta == -1)
    {
        perror("- semctl idSemaforoAccessoCodeMessaggi");
        exit(EXIT_FAILURE);
    }
    i = 0;
    for (i; i < puntatoreSharedMemoryTuttiNodi[0].nodoPid; i++)
    {
        msgctlRisposta = msgctl(puntatoreSharedMemoryTuttiNodi[i + 1].mqId, IPC_RMID, NULL);
        if (msgctlRisposta == -1)
        {
            perror("- msgctl"); /*essere piu' dettagliato*/
            exit(EXIT_FAILURE);
        }
        /*printf("+ codaMessaggi con ID %d eliminata con successo\n", puntatoreSharedMemoryTuttiNodi[i + 1]);*/
    }
    eseguiDetachShm(puntatoreSharedMemoryTuttiNodi, "puntatoreSharedMemoryTuttiNodi");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryTuttiNodi);
    if (shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }*/
    eseguiDeallocamentoShm(idSharedMemoryTuttiNodi, "idSharedMemoryTuttiNodi");
    /*shmctlRisposta = shmctl(idSharedMemoryTuttiNodi, IPC_RMID, NULL);
    if (shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryTuttiNodi");
        exit(EXIT_FAILURE);
    }*/
    semctlRisposta = semctl(idSemaforoAccessoIndiceLibroMastro, /*ignorato*/ 0, IPC_RMID);
    if (semctlRisposta == -1)
    {
        perror("semctl idSemaforoAccessoIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }
    eseguiDetachShm(puntatoreSharedMemoryIndiceLibroMastro, "puntatoreSharedMemoryIndiceLibroMastro");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryIndiceLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }*/
    eseguiDeallocamentoShm(idSharedMemoryIndiceLibroMastro, "idSharedMemoryIndiceLibroMastro");
    /*shmctlRisposta = shmctl(idSharedMemoryIndiceLibroMastro, IPC_RMID, NULL);
    if (shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryIndiceLibroMastro");
        exit(EXIT_FAILURE);
    }*/
    /*semctlRisposta = semctl(idSemaforoAccessoLibroMastro,0, IPC_RMID);
    if (semctlRisposta == -1)
    {
        perror("semctl idSemaforoAccessoLibroMastro");
        exit(EXIT_FAILURE);
    }*/
    eseguiDetachShm(puntatoreSharedMemoryLibroMastro, "puntatoreSharedMemoryLibroMastro");
    /*shmdtRisposta = shmdt(puntatoreSharedMemoryLibroMastro);
    if (shmdtRisposta == -1)
    {
        perror("- shmdt puntatoreSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }*/
    eseguiDeallocamentoShm(idSharedMemoryLibroMastro, "idSharedMemoryLibroMastro");
    /*shmctlRisposta = shmctl(idSharedMemoryLibroMastro, IPC_RMID, NULL);
    if (shmctlRisposta == -1)
    {
        perror("- shmctl idSharedMemoryLibroMastro");
        exit(EXIT_FAILURE);
    }*/
    printf("+ Risorse deallocate correttamente\n");
    return 0; /* == exit(EXIT_SUCCESS)*/
}

/*DEFINIZIONE DELLE FUNZIONI*/

int readAllParameters()
{
    char buffer[256]; /*={}*/
    char *token;
    char *delimeter = "= ";
    int parsedValue;
    FILE *configFile = fopen("parameters.conf", "r+");
    int closeResponse;

    bzero(buffer, 256);

    if (configFile != NULL)
    {
        /* printf("File aperto!\n");*/
        while (fgets(buffer, 256, configFile) != NULL) /*comparazione diretta tra fgets e 0 non concessa in pedantic*/
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
            /*else if (strcmp(token, "SO_TP_SIZE") == 0)
            {
                SO_TP_SIZE = atoi(strtok(NULL, delimeter));
            }
            else if (strcmp(token, "SO_BLOCK_SIZE") == 0)
            {
                SO_BLOCK_SIZE = atoi(strtok(NULL, delimeter));
            }*/
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
                /*exit(EXIT_FAILURE);*/
            }

            /*while(token != NULL)
            {
                printf("%s", token);
                token = strtok(NULL, "=");
            }*/
        }
        /* printf("\n");*/
        closeResponse = fclose(configFile);
        if (closeResponse == -1)
        {
            perror("fclose");
            return -1;
            /*exit(EXIT_FAILURE);*/
        }

/* PER TESTING */
#if (ENABLE_TEST)
        printf("Stampo i parametri parsati!\n");
        printf("SO_USERS_NUM=%d\n", getSoUsersNum());
        printf("SO_NODES_NUM=%d\n", getSoNodesNum());
        printf("SO_REWARD=%d\n", getSoReward());
        printf("SO_MIN_TRANS_GEN_NSEC=%ld\n", getSoMinTransGenNsec());
        printf("SO_MAX_TRANS_GEN_NSEC=%ld\n", getSoMaxTransGenNsec());
        printf("SO_RETRY=%d\n", getSoRetry());
        /*printf("SO_TP_SIZE=%d\n", SO_TP_SIZE);
        printf("SO_BLOCK_SIZE=%d\n", SO_BLOCK_SIZE);*/
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
        /*exit(EXIT_FAILURE);*/
    }
}

void alarmHandler(int sigNum)
{
    int cont;
    sigset_t set, setOld;
    cont = 0;
    printf("ALARM scattato - notifico i child e dealloco le risorse...\n");
    /*header della SM che contine tutti i pid rispecchia il numero dei nodi*/
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
    cont = 0;
    for (cont; cont < puntatoreSharedMemoryTuttiNodi[0].nodoPid; cont++)
    {
        kill(puntatoreSharedMemoryTuttiNodi[cont + 1].nodoPid, SIGUSR1);
        printf("NOTIFICO NODO: %d\n", puntatoreSharedMemoryTuttiNodi[cont + 1].nodoPid);
    }
    master = MASTER_STOP;
    motivoTerminazione = ALLARME_SCATTATO;
    /*mashero successivi ALARM*/
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    printf("+Maschera settata per futuri ALARM: %d\n", sigprocmask(SIG_BLOCK, &set, &setOld));
}

void stampaTerminale(int flag)
{

    int contatoreStampa;
    int contPremat;
    char *ragione;

    if (flag == 1)
    {
        ragione = "forza maggiore\0";
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
        default:
            break;
        }
        printf("Ragione della terminazione: %s\n", ragione);
    }

    contPremat = 0;
    /*stampo bilancio utenti*/
    contatoreStampa = 0;

    utenteMax.budget = puntatoreSharedMemoryTuttiUtenti[1].budget;
    utenteMin.budget = puntatoreSharedMemoryTuttiUtenti[1].budget;
    nodoMax.budget = puntatoreSharedMemoryTuttiNodi[1].budget;
    nodoMin.budget = puntatoreSharedMemoryTuttiNodi[1].budget;
    if (getSoUsersNum() < 2000)
    {
        printf("UTENTE[PID] | BILANCIO[INT] | STATO\n");
        for (contatoreStampa; contatoreStampa < getSoUsersNum(); contatoreStampa++)
        {
            printf("%09d\t%09d\t%09d\n", puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].userPid, puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].budget, puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].stato);
            if (puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].stato == USER_KO)
            {
                contPremat++;
            }
        }
    }
    else
    {
        /* utenteMax.budget = 0;
         utenteMin.budget = SO_BUDGET_INIT;*/

        for (contatoreStampa; contatoreStampa < getSoUsersNum(); contatoreStampa++)
        {
            if (puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].budget > utenteMax.budget)
            {
                utenteMax = puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1];
            }
            if (puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].budget < utenteMin.budget)
            {
                utenteMin = puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1];
            }
            if (puntatoreSharedMemoryTuttiUtenti[contatoreStampa + 1].stato == USER_KO)
            {
                contPremat++;
            }
        }
        /* if (contPremat >= SO_USERS_NUM -1){
             utenteMax.budget = 0;
             utenteMin.budget = 0;
             utenteMax.userPid = 0;
             utenteMin.userPid = 0;
             printf("TERMINAZIONE UTENTI\n");
         }*/
        printf("UTENTE[PID] | BILANCIO[INT] | STATO\n");
        printf("%09d\t%09d\t%09d <-- UTENTE con budget MAGGIORE\n", utenteMax.userPid, utenteMax.budget, utenteMax.stato);
        printf("%09d\t%09d\t%09d <-- UTENTE con budget MINORE\n", utenteMin.userPid, utenteMin.budget, utenteMin.stato);
    }
    if (flag == 1)
    {
        printf("*******\n# Utenti terminati prematuramente: %d / %d\n**********\n", contPremat, getSoUsersNum());
        master = MASTER_STOP;
    }
    contatoreStampa = 0;
    if (getSoNodesNum() < 200000)
    {
        printf("NODO[PID] | BILANCIO[INT] | TRANSAZIONI PENDENTI\n");
        for (contatoreStampa; contatoreStampa < puntatoreSharedMemoryTuttiNodi[0].nodoPid; contatoreStampa++)
        {
            printf("%09d\t%09d\t%09d\n", puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].nodoPid, puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].budget, puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].transazioniPendenti);
        }
        printf("TOTALE NODI: %d\n", puntatoreSharedMemoryTuttiNodi[0].nodoPid);
    }
    else
    {
        for (contatoreStampa; contatoreStampa < puntatoreSharedMemoryTuttiNodi[0].nodoPid; contatoreStampa++)
        {
            /* nodoMax.budget = 0;
             nodoMin.budget = SO_BUDGET_INIT;*/
            if (puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].budget > nodoMax.budget && puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].budget != 0)
            {
                nodoMax = puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1];
            }
            if (puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1].budget < nodoMin.budget)
            {
                nodoMin = puntatoreSharedMemoryTuttiNodi[contatoreStampa + 1];
            }
        }
        printf("NODO[PID] | BILANCIO[INT] | TRANSAZIONI PENDENTI\n");
        printf("%09d\t%09d\t%09d <-- NODO con budget MAGGIORE\n", nodoMax.nodoPid, nodoMax.budget, nodoMax.transazioniPendenti);
        printf("%09d\t%09d\t%09d <-- NODO con budget MINORE\n", nodoMin.nodoPid, nodoMin.budget, nodoMin.transazioniPendenti);
        printf("TOTALE NODI: %d\n", puntatoreSharedMemoryTuttiNodi[0].nodoPid);
    }
    printf("*******\nNumero di blocchi: %d\n", *(puntatoreSharedMemoryIndiceLibroMastro));
}

void outputLibroMastro()
{
    FILE *out;
    out = fopen("./registro.dat", "w+");
    if (out == NULL)
    {
        perror("fopen: outputLibroMastro");
        exit(EXIT_FAILURE);
    }

    char buffer[64];
    bzero(buffer, 64);

    for (int i = 0; i < *puntatoreSharedMemoryIndiceLibroMastro; i++)
    {
        sprintf(buffer, "| RECEIVER(PID)| SENDER(PID)  | QUANTITA     | TIMESTAMP\n");
        fputs(buffer, out);
        for (int j = 0; j < getSoBlockSize(); j++)
        {
            sprintf(buffer, "| %013d| %013d| %013d| %s", puntatoreSharedMemoryLibroMastro[getSoBlockSize() * i + j].receiver, puntatoreSharedMemoryLibroMastro[getSoBlockSize() * i + j].sender, puntatoreSharedMemoryLibroMastro[getSoBlockSize() * i + j].quantita, ctime(&puntatoreSharedMemoryLibroMastro[getSoBlockSize() * i + j].timestamp.tv_sec));
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

    printf(" __   __   __    ___ ___       __                  __   __   __                 __  ___  __   __     ___  ___  __                    ___       \n");
    printf("/__` /  ` |__) |  |   |  |  | |__)  /\\     |    | |__) |__) /  \\     |\\/|  /\\  /__`  |  |__) /  \\     |  |__  |__)  |\\/| | |\\ |  /\\   |   /\\  .\n");
    printf(".__/ \\__, |  \\ |  |   |  \\__/ |  \\ /~~\\    |___ | |__) |  \\ \\__/     |  | /~~\\ .__/  |  |  \\ \\__/     |  |___ |  \\  |  | | | \\| /~~\\  |  /~~\\ .\n");
    printf("                                                                                                                                               \n");
    printf("   /  __   ___  __     __  ___  __   __    __       ___                                                                                        \n");
    printf("  /  |__) |__  / _` | /__`  |  |__) /  \\  |  \\  /\\   |                                                                                         \n");
    printf("./   |  \\ |___ \\__> | .__/  |  |  \\ \\__/ .|__/ /~~\\  |                                                                                         \n");
}

void aggiungiAmico(int newPid, int numeroOrdine)
{
    /*contatore per SO_FRIENDS_NUM*/
    int c;
    /*contatore per posti disponibi per utente*/
    int ci;
    /*ciclo per SO_FRINEDS_NUM volte*/
    for(c = 0; c < getSoFriendsNum(); c++)
    {
        /*se nodo chelto non acetta piu' amici lo ignoro*/
        if(puntatoreSharedMemoryAmiciNodi[nextFriend*(2*getSoFriendsNum() + 1)] == 2 * getSoFriendsNum())
        {
            printf("Nodo %d non accetta piu' amici!\n", puntatoreSharedMemoryTuttiNodi[nextFriend + 1].nodoPid);
        }
        else
        {
            /*altrimenti cerco la prima cella utile nel suo pool di amici e aggiungo un nodo*/
            for(ci = 1; ci <= 2 * getSoFriendsNum(); ci++)
            {
                if(puntatoreSharedMemoryAmiciNodi[nextFriend*(2*getSoFriendsNum() + 1) + ci] == -1)
                {
                    puntatoreSharedMemoryAmiciNodi[nextFriend*(2*getSoFriendsNum() + 1) + ci] = numeroOrdine;
                    puntatoreSharedMemoryAmiciNodi[nextFriend*(2*getSoFriendsNum() + 1)] += 1;/*incremento di 1 il numero degli amici*/
                    nextFriend++;
                    if((nextFriend + 1) == puntatoreSharedMemoryTuttiNodi[0].nodoPid)
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