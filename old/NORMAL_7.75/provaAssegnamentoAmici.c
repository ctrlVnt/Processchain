#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char const *argv[])
{
    int nodi[11] = {-1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int *amici = calloc((5*2 + 1)*10, sizeof(int));
    memset(amici, -1, sizeof(int) * (5*2 + 1)*10);/**/
    printf("Riservo spazio per amici\n");
    for(int k = 0; k < 10; k++)
    {
        for(int g = 0; g < (2*5); g++)
        {
            printf("%d\t", amici[(k*5*2)+g]);
        }
        printf("\n");
    }
    // return 0;
    printf("Ho a disposizione 10 nodi:\n");
    int indiceAmicoSuccessivo = 2;
    /*per ciascun nodo assegno*/
    for(int i = 0; i < 10; i++)
    {
        amici[i*(5*2 + 1)] = 999;
        /*assrgno amici*/
        for(int t = 1; t <= (5); t++)
        {
            /*         [ blocco ]*/
            amici[(i)* (5 * 2 +1) + t] = nodi[indiceAmicoSuccessivo++];
            if(indiceAmicoSuccessivo == 11)
            {
                indiceAmicoSuccessivo = 1;
            }
        }
    }
    /*stampo gli amici*/
    for(int k = 0; k < 10; k++)
    {
        printf("NODO %d : \t%d|\tAMICI:\t", (k + 1), amici[k*(5*2 + 1)]);
        for(int g = 1; g <= 5*2; g++)
        {
            printf("%d\t", amici[(k*(5*2 + 1))+g]);
        }
        printf("\n");
    }
    return 0;
}
