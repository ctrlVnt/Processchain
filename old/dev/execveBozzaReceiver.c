#include <stdio.h>
#include <string.h>

int main(int argc, char const *argv[])
{
    printf("CIAO !!!!\nHo %d parametri!\n", argc);
    /* code */
    for(int i = 0; i < argc; i++){
        printf("%s\n", argv[i]);
    }
    printf("Fine parametri!\nPosso attacarmi alle memorie condivise!\n");
    return 0;
}
