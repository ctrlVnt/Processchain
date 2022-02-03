CC = gcc
STD = -std=c89
PDNTC = -pedantic
CCSTDPDNTC = gcc -std=c89 -pedantic

all: NodoBozza.out UtenteBozza.out MasterBozza.out

MasterBozza.out: ./headers/process_chain.c ./headers/customs.c ./entities/Master.c
	${CCSTDPDNTC} $^ -o $@

UtenteBozza.out: ./headers/process_chain.c ./headers/process_chain_with_utils.c ./entities/Utente.c
	${CCSTDPDNTC} $^ -o $@

NodoBozza.out: ./headers/process_chain.c ./headers/process_chain_with_utils.c ./entities/Nodo.c
	${CCSTDPDNTC} $^ -o $@

help: 
	@echo 	"Makefile help:\n \
			* all        	: compiles the programs and creates the executable files and object files\n \
			* play 		: compiles the programs and creates the executable files and \n\t\t\t\t  object files, execute program and remove files used during execution\n \
			* clean        	: removes all file created during any kind of build operation\n \
			* help         	: print this file\n \
			* sim1          : execute configuration 1\n \
			* sim2        	: execute configuration 2\n \
			* sim3		: execute configuration 3\n \
			* cleanall	: removes all file created during any kind of build operation and register\n\n\
	For parameters configuration: \n\
			+ SO_TP_SIZE and SO_BLOCK_SIZE in folder headers -> process_chain.c\n\
			+ the other parameters in folder conf -> parameters.conf\n"

play: all run clean

run:
	./MasterBozza.out ./conf/parameters.conf

run1:
	./MasterBozza.out ./conf/parameters.1.conf

run2:
	./MasterBozza.out ./conf/parameters.2.conf

run3:
	./MasterBozza.out ./conf/parameters.3.conf
	
sim1: all run1 clean

sim2: all run2 clean

sim3: all run3 clean

clean: 
	rm -f *.out *.o
cleanall:
	rm -f *.out *.o registro.dat
