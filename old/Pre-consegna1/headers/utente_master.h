/*Permette di discriminare gli utenti, 1 - utente ancora attivo*/
#define USER_OK 1
/*Permette di discriminare gli utenti, 0 - utente non e' attivo*/
#define USER_KO 0
/*???*/
/*
Utilizzata nel ciclo d'attesa di terminazione degli utenti.
Se un utente termina prematuramente, questo e' il suo valore di ritorno.
*/
#define EXIT_PREMAT 1
/**/
/*Le macro che definiscono diversi ragioni della terminazione della simulazione*/
#define ALLARME_SCATTATO 0
#define NO_UTENTI_VIVI 1
#define REGISTRY_FULL 2
#define TERMINATO_DA_UTENTE 3
#define LIMITE_SISTEMA 4
