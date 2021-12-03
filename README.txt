PROCESS-CHAIN

Il progetto consiste nel gestire i pagamenti con libro mastro tra due utenti.

File manager: che si ocuperà di ricevere le richieste di transazione dal pagante,
inviare la transazione al generatoredi chiavi in modo da rendere distinto e non tracciabile
ogni pagamento, scrivere sul file pipe la transazione e aggiornare il portafoglio del ricevente
e del pagante a transazione scritta.

File pagante: chi effettua il pagamento deve inserire prima il suo conto, poi deve creare un
processo per ogni transazione che è intenzionato ad eseguire.

File generatore di chiavi: riceve dal master la richiesta, crea la chiave univoca del pagamento
e invia nuovamente la transazione processata al master.

File ricevente: rimane in attesa dei soldi, non è necessario che inserisca un badget iniziale
ma si potrebbe anche inserire un metodo, per esempio una RATEIZZAZIONE.

SO_BLOCK_SIZE: numero di transazioni contenute in un blocco //DOMANDA DI ESAME