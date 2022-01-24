all:  master

MasterBozzaModificato27Dic.o : UtenteBozzaModificato27Dic.o NodoBozzaModificato27Dic.o

master: MasterBozzaModificato27Dic.c UtenteBozzaModificato27Dic.c NodoBozzaModificato27Dic.c
	gcc MasterBozzaModificato27Dic.c  -o master 

bozza:	UtenteBozzaModificato27Dic NodoBozzaModificato27Dic
	gcc MasterBozzaModificato27Dic.c -o MasterBozza

UtenteBozzaModificato27Dic:
	gcc UtenteBozzaModificato27Dic.c -o UtenteBozza

NodoBozzaModificato27Dic:
	gcc NodoBozzaModificato27Dic.c -o NodoBozza

run:  all
	./MasterBozza

clean: 
	rm master