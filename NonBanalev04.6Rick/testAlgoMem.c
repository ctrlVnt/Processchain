#include <stdio.h>
/*int probNodNum(int nodi, int amici, int tp, int block){
    int multmem = 0;
    multmem = nodi / amici;
    multmem = 100 * multmem;
    multmem = multmem / tp;
    multmem = multmem * (block / 2);
    return multmem;
}*/

/*int probNodNum(int utenti, int nodi, int amici, int tp, int block){
    int multmem = 0;
    multmem = utenti / nodi;
    multmem = multmem / amici;
    multmem = 100 * multmem;
    tp = tp / 3;
    multmem = multmem / tp;
    multmem = multmem * (block / 2);
    return multmem;
}*/

/*int probNodNum(int utenti, int nodi, int amici, int tp, int retry, int hops){
    int multmem = 0;
    int num = nodi + amici;
    int divisore = retry + hops;
    while (num < tp){
        num = num * 10;
    }
    while (utenti < (num / tp))
    {
        utenti = utenti * 10;
    }
    
    multmem = ((utenti * (num / tp))) / divisore;
    return multmem;
}*/

/*float probNodNum(float utenti, float nodi, float tp, float retry){
    float x = ((utenti / nodi) - (tp / retry));
    return x;
}*/

float probNodNum(float utenti, float nodi, float tp, float sohops){
    float x = (utenti / nodi);
        if(tp > sohops){
        tp = tp / sohops;
        }else{
            tp = (sohops / tp) * 2;
        }
        x = x / tp;
        if (x <= 1){
            x = 2;
        }else if (x >= 100){
            x = 10;
        }
    return x;
}

int main(){
    printf("alloco memoria: %f\n", probNodNum(500, 30, 10, 2));
}

/*Assumo che verranno creati almeno 25 nodi (100 / 4 byte)
* il conto considera un calcolo grossolano:
* più utenti hai su nodi e più devi creare nodi
* più hai la transation pool grande e meno transazioni creerai per la coda occupata
* meno retry hai e più creerai transazioni
 */