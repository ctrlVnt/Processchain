#include <errno.h>

int main(){
    int i = 0;
    int child;
    while (child != EAGAIN)
    {
        child = fork();

        switch (child)
        {
        case -1:
            perror("cazzo\n");
            break;
        case 0:
            printf("sono nato\n");
            break;
        default:
        i++;
            break;
        }
    }
    printf("ho creato %d fork\n", i);
    
}