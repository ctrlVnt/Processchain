#include "process_chain.h"

/*funzione permette di scegliere amico nodo*/
int scegliNumeroNodo(int max);
/*restiruisce la posizione o indice del nodo amico nellla shared memory principale*/
int scegliAmicoNodo(int idNodo, int *puntatoreSharedMemoryAmiciNodi);