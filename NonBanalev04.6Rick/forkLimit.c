#include <errno.h>
#include <stdlib.h>

int main(){
    int i = 0;
    int child;
    while (child != EAGAIN)
    {
        i++;
        child = fork();

        switch (child)
        {
        case -1:
            printf("cazzo %d\n", i);
            sleep(1000);
            break;
        case 0:
            printf("sono nato %d\n", i);
            sleep(1000);
            exit(EXIT_SUCCESS);
            break;
        default:
            break;
        }
    }
    printf("ho creato %d fork\n", i);
    
}