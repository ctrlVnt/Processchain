if(regalo)
                {

                    if(inviato)
                    {
                        /*regalo inviato*/
                        mRegalo.transazione = messaggioRicevuto.transazione;
                        mRegalo.hops = messaggioRicevuto.hops;
                        /*idCoda = scegliAmicoNodo(idCoda);
                        semopRisposta = semReserve(idMiaMiaCodaMessaggi, -1, (idCoda - 1), IPC_NOWAIT);
                        if(semopRisposta == -1 && errno == EAGAIN)
                        {
                            inviato = 0;
                            mRegalo.hops--;
                            if(mRegalo.hops == 0)
                            {
                                msgsnd(idCodaMessaggiProcessoMaster, &mRegalo, sizeof(mRegalo.transazione) + sizeof(mRegalo.hops), 0);
                            }
                        }
                        else
                        {
                            msgsnd(puntatoreSharedMemoryTuttiNodi[idCoda].mqId, &mRegalo, sizeof(mRegalo.transazione) + sizeof(mRegalo.hops), 0);
                            inviato = 1;
                        }*/
                    }

                    /*scelgo amico*/
                    idCoda = scegliAmicoNodo(idCoda);
                    semopRisposta = semReserve(idMiaMiaCodaMessaggi, -1, (idCoda - 1), IPC_NOWAIT);
                    if(semopRisposta == -1 && errno == EAGAIN)
                    {
                        inviato = 0;
                        mRegalo.hops--;
                        if(mRegalo.hops == 0)
                        {
                            msgsnd(idCodaMessaggiProcessoMaster, &mRegalo, sizeof(mRegalo.transazione) + sizeof(mRegalo.hops), 0);
                        }
                    }
                    else
                    {
                        printf("REGALO\n%d, %d, %d\n", mRegalo.transazione.receiver, mRegalo.transazione.sender, mRegalo.transazione.quantita);
                        msgsnd(puntatoreSharedMemoryTuttiNodi[idCoda].mqId, &mRegalo, sizeof(mRegalo.transazione) + sizeof(mRegalo.hops), 0);
                        inviato = 1;
                    }
                    indiceSuccessivoTransactionPool++;
                }