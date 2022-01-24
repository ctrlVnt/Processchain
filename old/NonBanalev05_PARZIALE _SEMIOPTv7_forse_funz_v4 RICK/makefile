CC = gcc

#quando introdurremo pedantic cambierà perché bisognerà prima compilare i file, ma basterà aggiungere questa etichetta con le corrette dipendenze
CFLAGS=  -std=c89 -Wpedantic

all: NodoBozza.out UtenteBozza.out MasterBozza.out

MasterBozza.out:	process_chain.c Master.c
	$(CC) $^ -o $@

UtenteBozza.out:	process_chain.c Utente.c
	$(CC) $^ -o $@

NodoBozza.out: Nodo.c process_chain.c
	$(CC) $^ -o $@

play: all run clean

run:
	./MasterBozza.out

clean: 
	rm -f *.out *.o