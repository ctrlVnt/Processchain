CC = gcc
STD = -std=c89
PDNTC = -pedantic
CCSTDPDNTC = gcc -std=c89 -pedantic

all: NodoBozza.out UtenteBozza.out MasterBozza.out

MasterBozza.out: process_chain.c process_chain_utils.c Master.c
	${CCSTDPDNTC} $^ -o $@

UtenteBozza.out: process_chain.c Utente.c
	${CCSTDPDNTC} $^ -o $@

NodoBozza.out: Nodo.c process_chain.c
	${CCSTDPDNTC} $^ -o $@

play: all run clean

run:
	./MasterBozza.out parameters.conf

sim1:
	./MasterBozza.out parameters.1.conf

sim2:
	./MasterBozza.out parameters.2.conf

sim3:
	./MasterBozza.out parameters.3.conf

clean: 
	rm -f *.out *.o