all: Nodo Utente Master

Master:	process_chain.o Master.o
	gcc process_chain.o Master.o -o MasterBozza.out

Master.o: Master.c
	gcc -c Master.c

Utente:	process_chain.o Utente.o
	gcc process_chain.o Utente.o -o UtenteBozza.out

Utente.o: Utente.c
	gcc -c Utente.c

Nodo: Nodo.o process_chain.o
	gcc process_chain.o Nodo.o -o NodoBozza.out

Nodo.o: Nodo.c
	gcc -c Nodo.c

process_chain.o: process_chain.c
	gcc -c process_chain.c

run:  
	./MasterBozza.out

clean: 
	rm -f *.out *.o
