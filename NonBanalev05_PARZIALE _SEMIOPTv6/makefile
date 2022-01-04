all: NodoBozza.out UtenteBozza.out MasterBozza.out

MasterBozza.out:	process_chain.c Master.c
	gcc process_chain.c Master.c -o MasterBozza.out

UtenteBozza.out:	process_chain.c Utente.c
	gcc process_chain.c Utente.c -o UtenteBozza.out

NodoBozza.out: Nodo.c process_chain.c
	gcc process_chain.c Nodo.c -o NodoBozza.out

play: all run clean

run:
	./MasterBozza.out

clean: 
	rm -f *.out *.o