CONSIGLI DEL PROFESSORE

SO_BLOCK_SIZE: numero di transazioni contenute in un blocco //DOMANDA DI ESAME

Non ha senso eliminare e ricompilare tutti i file oggetto ma ricompilare solo i file modificato //ERRORE TIPICO

nela compilazione gcc "-D" può essere utile, utilzzarlo

definire macro DEBUG può essere utile

//remember: dobbiamo ricordarci che in ogni momento il processo può ricevere un segnale eseguendo l'handler di quel segnale risponde e, 
dopo l'esecuzione, il processo continua ad eseguire il programma:
1 - se non imposto l'handler viene eseguito un comportamento di default
2 - può terminare o bloccare il programma
soluzione? impostare handler (sigaction)

pipes: da utilizzare quand voglio far comunicare velocemente padre e figlio o più figli

code di messaggi: servono per fare comunicare più processi non correlati tra di loro, esempio della codifica si può scrivere un messaggio e leggerla solo successivamente

cosa metto nell'hidden file? Definisco la struttura di un messaggio (primo parametro long)

praticamente SICURO bisognerà usare un semaforo per la sincronizzazione

come gestire un semafoto? noi utiliziamo la system call semop(*funzioni) alcuni flag possono rendere la semop NON bloccante
la semop non è bloccante se le 4 azioni non vengono eseguite insieme

//DOMANDA DI ESAME: ma tra lasciare memori attack e memory get qual'è la system call che effettivamente alloca la memoria?
SHMGET perché shmattack attacca lka memoria GIA allocata

ogni volta che abbiamo una system call bloccante facciamoci la domanda:
ma cosa succede se quando il processo è bloccato in quella system call mi arriva un segnale?
LA SYSTEM CALL SI SBLOCCA, RESTITUISCE -1 E IN ERRNO AVETE IL CORRISPONDENTE VALORE DI ERRORE
SEMPRE discriminare se si è interrotto per errore o perché si è sbloccata bene