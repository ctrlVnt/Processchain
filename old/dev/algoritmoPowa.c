#include <stdio.h>
#include <stdlib.h>

int algoritmoNonCosiPowa2(int SO_BLOCK_SIZE, int SO_NODES_NUM, int SO_USERS_NUM);

int algoritmoNonCosiPowa(int SO_BLOCK_SIZE, int SO_NODES_NUM, int SO_USERS_NUM);

int main(){
	int SO_BLOCK_SIZE = 10;
	int SO_USERS_NUM = 20;
	int SO_NODES_NUM = 10;
	int SO_TP_SIZE;
	
	printf("%d\n",algoritmoNonCosiPowa(SO_BLOCK_SIZE, SO_NODES_NUM, SO_USERS_NUM));
}
/*versione "da usare" se teniamo il fatto che riempiamo il blocco solo a TP piena*/
int algoritmoNonCosiPowa(int SO_BLOCK_SIZE, int SO_NODES_NUM, int SO_USERS_NUM){
	if((SO_BLOCK_SIZE * SO_NODES_NUM) < SO_USERS_NUM){
		return SO_USERS_NUM/SO_BLOCK_SIZE;   //dimensione array nodi da allocare
	}else{
		return SO_BLOCK_SIZE*SO_NODES_NUM;  //SO_USERS_NUM/SO_TP_SIZE;
	}
}

/*versione "da usare" se teniamo il fatto che riempiamo il blocco quando abbiamo un EAGAIN */
int algoritmoNonCosiPowa2(int SO_BLOCK_SIZE, int SO_NODES_NUM, int SO_USERS_NUM){
	if((SO_BLOCK_SIZE * SO_NODES_NUM) < SO_USERS_NUM){
		return SO_USERS_NUM/SO_BLOCK_SIZE;   //dimensione array nodi da allocare
	}else{
		return SO_BLOCK_SIZE*SO_NODES_NUM;  //SO_USERS_NUM/SO_TP_SIZE;
	}
}

/*Nella versione normal bisogna rilevare il caso in cui una fork fallisce a causa del numero eccessivo di nodi e 
  quindi agire in qualche modo*/
