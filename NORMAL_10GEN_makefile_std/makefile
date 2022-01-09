STD = -std=c89
PDNTC = -pedantic

all: NodoBozza.out UtenteBozza.out MasterBozza.out

MasterBozza.out: process_chain.c process_chain_utils.c Master.c
	gcc ${STD} ${PDNTC} process_chain.c process_chain_utils.c Master.c -o MasterBozza.out

UtenteBozza.out: process_chain.c Utente.c
	gcc ${STD} ${PDNTC} process_chain.c Utente.c -o UtenteBozza.out

NodoBozza.out: Nodo.c process_chain.c
	gcc ${STD} ${PDNTC} process_chain.c Nodo.c -o NodoBozza.out

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