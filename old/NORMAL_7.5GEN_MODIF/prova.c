#include <stdio.h>

int main(int argc, char const *argv[])
{
    int i, i2;
    i = 0;
    i2 = 0;
    while(i < 10)
    {
        printf("Ciclo esterno i == %d\n", i);
        while(i2 < 20){
            printf("Ciclo interno con i2 == %d\n", i2);
            if(i2 == 10)
            {
                printf("esco ciclo interno i2 == 10\n");
                break;
            }
            i2++;
        }
        i++;
    }
    return 0;
}
