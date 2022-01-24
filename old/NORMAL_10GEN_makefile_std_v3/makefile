CC = gcc
STD = -std=c89
PDNTC = -pedantic
CCSTDPDNTC = gcc -std=c89 -pedantic

all: NodoBozza.out UtenteBozza.out MasterBozza.out

MasterBozza.out: process_chain.c process_chain_custom.c Master.c
	${CCSTDPDNTC} $^ -o $@

UtenteBozza.out: process_chain.c Utente.c
	${CCSTDPDNTC} $^ -o $@

NodoBozza.out: process_chain.c Nodo.c
	${CCSTDPDNTC} $^ -o $@

help: 
	@echo 	"Makefile help:\n \
			* all        	: compiles the programs and creates the executable files and object files\n \
			* play 		: compiles the programs and creates the executable files and object files, execute program and remove files used during execution\n \
			* clean        	: removes all file created during any kind of build operation\n \
			* help         	: print this file\n \
			* sim1          : execute configuration 1\n \
			* sim2        	: execute configuration 2\n \
			* sim3		: execute configuration 3"

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