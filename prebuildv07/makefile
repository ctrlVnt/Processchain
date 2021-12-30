all:  master

master: MasterBozzaModificato27Dic.c UtenteBozzaModificato27Dic.c NodoBozzaModificato27Dic.c
	gcc MasterBozzaModificato27Dic.c  -o master 

bozza:	Utente Nodo process_chain
	gcc process_chain.o Master.c -o MasterBozza.out

Utente:	process_chain
	gcc Utente.c -o UtenteBozza.out

Nodo:	process_chain
	gcc Nodo.c -o NodoBozza.out

process_chain:
	gcc -c process_chain.c

run:  all
	./MasterBozza

clean: 
	rm -f *.out *.o
