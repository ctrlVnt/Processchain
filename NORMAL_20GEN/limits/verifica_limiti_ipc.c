#include "../headers/process_chain.h"

int main(int argc, char const *argv[])
{
    int mq_id;
    int shm_id;
    int sem_id;
    struct msqid_ds mq_info;
    struct shmid_ds shm_info;
    struct shmid_ds shm_info_linux_spec;
    /*struct semid_ds sem_info;*/ /*non utilizzato*/
    struct semid_ds sem_info_linux_spec;
    message m;
    /*int counter = 0;*/

    printf("\n\033[1;31mLimiti IPC V5:\033[0m\n\n");
    mq_id = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    msgctl(mq_id, IPC_STAT, &mq_info);
    printf("\033[1;34mCODE DI MESSAGGI:\033[0m\n");
    printf("Su questa macchina una MQ puo' contenere al massimo \033[1;37m%ld\033[0m bytes, che corrispondono a circa \033[1;37m%ld\033[0m messaggi, dato che un messaggio da noi definito occupa \033[1;37m%ld\033[0m bytes\n", mq_info.msg_qbytes, mq_info.msg_qbytes / (sizeof(m.transazione) + sizeof(m.hops)), sizeof(m.hops) + sizeof(m.transazione));
    printf("Mentre linux specific dice che possono essere scritti in una coda e' %d\n\n", ((struct msginfo*)&mq_info)->msgmnb);
    /*
    mq_info.msg_qbytes = 1000 * (sizeof(m.hops) + sizeof(m.transazione));
    */
    /*
    msgctl(mq_id, IPC_SET, &mq_info);
    */
    /*
    while(msgsnd(mq_id, &m, sizeof(m.hops) + sizeof(m.transazione), 0) != -1)
    {
        printf("Scrivo %d messaggi\n", ++counter);
    }
    */
    msgctl(mq_id, IPC_RMID, NULL);
    printf("\033[1;34mSHARED MEMORY:\033[0m\n");
    shm_id = shmget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    shmctl(shm_id, IPC_STAT, &shm_info);
    printf("Su questa macchina una SHM ha la lunghezza di uno segmento pari a \033[1;37m%ld\033[0m bytes di default\n ", shm_info.shm_segsz);
    shmctl(shm_id, IPC_INFO, &shm_info_linux_spec);
    printf("Una SHM puo' essere composta al massimo da \033[1;37m%ld\033[0m segmenti, ciascuno che puo' occupare al massimo %ld bytes.\n\n", ((struct shminfo*)&shm_info_linux_spec)->shmmni, ((struct shminfo*)&shm_info_linux_spec)->shmmax);
    shmctl(shm_id, IPC_RMID, NULL);
    printf("\033[1;34mSEMSETS:\033[0m\n");
    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    semctl(sem_id, 0, IPC_INFO, &sem_info_linux_spec, NULL);
    printf("Un semset puo' contenere al massimo \033[1;37m%d\033[0m semafori\nIn assoluto possono essere creati \033[1;37m%d\033[0m semafori e al massimo \033[1;37m%d\033[0m semsets\n\n", ((struct seminfo*)&sem_info_linux_spec)->semmsl, ((struct seminfo*)&sem_info_linux_spec)->semmns, ((struct seminfo*)&sem_info_linux_spec)->semmni);
    semctl(sem_id, 0, IPC_RMID, NULL);
    return 0;
    
}
