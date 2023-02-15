# Operating systems project

## What is that?

University project for process synchronization within a UNIX system.

### About

We intend to simulate a ledger containing the data of monetary transactions between different users.
To that end they are have the following processes:
- **master** process that manages the simulation, the creation of other processes, etc.
- **user** processes that can send money to other users through a transaction
- **node** processes that process the received transactions for a fee.

#### Users
User processes are responsible for creating and sending money transactions to node processes. Every
user process is assigned an initial budget SO_BUDGET_INIT. During its lifecycle, a user process
iteratively performs the following operations:
- Calculate the current budget starting from the initial budget and making the algebraic sum of the income and the issues posted to transactions in the ledger, by     subtracting the shipped transaction amounts but not yet recorded in the ledger.
- It sends the transaction to the extracted node and waits for a randomly extracted time interval (in nanoseconds) between SO_MIN_TRANS_GEN_NSEC and maximum SO_MAX_TRANS_GEN_NSEC.

#### Nodes
Each node process privately stores the list of received transactions to process, called transaction pool, which can contain at most SO_TP_SIZE transactions, with SO_TP_SIZE > SO_BLOCK_SIZE. If the transaction node's pool is full, then any further transactions are discarded and therefore not executed. In this case, the sender of the rejected transaction must be informed.
Transactions are processed by a node in blocks. Each block contains exactly SO_BLOCK_SIZE transations to be processed of which SO_BLOCK_SIZE−1 transactions received from users and one payment transaction per processing (see below).
The life cycle of a node can be defined as follows:
- Creating a candidate block
- Simulates the processing of a block through an inactive wait for an expressed random time interval in nanoseconds between SO_MIN_TRANS_PROC_NSEC and SO_MAX_TRANS_PROC_NSEC.
- Once the processing of the block is completed, it writes the new block just processed in the ledger, ed deletes successful transactions from the transaction pool.
- periodically each node selects a transaction from the transaction pool that is not yet present in the ledger and sends it to a randomly selected friendly node (the transaction is deleted from the transaction pool of the source node)

## Where to find it?
There are two folders:
- the "old" folder contains previous lyrics and various previously used files.
- in the "Prochesschain" folder you will find the project with instructions for the test.

## Credits
Project made by: @Cosmok21 @botu27 @ctrlVnt

**Thanks for visit, have fun!**
