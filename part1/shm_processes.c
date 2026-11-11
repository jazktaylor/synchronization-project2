/* Jaz Taylor */

#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

typedef struct {
  int BankAccount;
  sem_t mutex;
} SharedData;

volatile sig_atomic_t stop = 0;

void  ClientProcess(SharedData *, int);
void DadProcess(SharedData *);
void MomProcess(SharedData *);

void handleSignal(int signal)
{
  (void) signal;
  stop = 1;
}

int main(int argc, char *argv[])
{
     int    ShmID;
     SharedData    *ShmPTR;
     pid_t  pid;
     pid_t momPID = -1;
     pid_t *childPIDs;
     int    status;
     int i;
     long parentCount;
     long studentCount;
     char *endptr;

     /* Validate command-line arguments */
     if (argc != 3) {
      printf("Usage: %s <parent> <children>\n", argv[0]);
      printf("Parents must be 1 or 2, and children must be greater than 0.\n");
      return 1;
     }

     errno = 0;
     parentCount = strtol(argv[1], &endptr, 10);

     if (errno != 0 || *endptr != '\0' || (parentCount != 1 && parentCount != 2)) {
      printf("Error: parents must be either 1 or 2.\n");
      return 1;
     }

     errno = 0; 
     studentCount = strtol(argv[2], &endptr, 10);

     if (errno != 0 || *endptr != '\0' || studentCount < 1 || studentCount > INT_MAX) {
      printf("Error: children must be a positive integer.\n");
      return 1;
     }

     printf("Number of parents: %ld\n", parentCount);
     printf("Number of students: %ld\n", studentCount);

     signal(SIGINT, handleSignal);
     signal(SIGTERM, handleSignal);

     ShmID = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
     if (ShmID < 0) {
          printf("*** shmget error (server) ***\n");
          exit(1);
     }
     printf("Server has received a shared memory for BankAccount and semaphore...\n");

     ShmPTR = (SharedData *) shmat(ShmID, NULL, 0);

     if (ShmPTR == (SharedData *) -1) {
          printf("*** shmat error (server) ***\n");
          shmctl(ShmID, IPC_RMID, NULL);
          exit(1);
     }
     printf("Server has attached the shared memory...\n");

     ShmPTR->BankAccount= 0;

     if (sem_init(&ShmPTR->mutex, 1, 1) == -1) {
      perror("sem_init");
      shmdt((void *) ShmPTR);
      shmctl(ShmID, IPC_RMID, NULL);
      exit(1);
     }
  
     printf("Server has initialized BankAccount to $%d\n",
            ShmPTR->BankAccount);
      
     /* Create Lovable Mom if two parents were requested */
     if (parentCount == 2) {

      printf("Server is about to fork Lovable Mom...\n");

      momPID = fork();

      if (momPID < 0) {
        perror("fork");

        sem_destroy(&ShmPTR->mutex);
        shmdt((void *) ShmPTR);
        shmctl(ShmID, IPC_RMID, NULL);
        return 1;
      }
      else if (momPID == 0) {
        MomProcess(ShmPTR);
        exit(0);
      }
     }

     /* Allocate space for Student process IDs */
     childPIDs = malloc((size_t) studentCount * sizeof(pid_t));

     if (childPIDs == NULL) {
      perror("malloc");

      /* Terminate and wait for Mom if she was created */
      if (momPID > 0) {
        kill(momPID, SIGTERM);
        waitpid(momPID, &status, 0);
      }

      sem_destroy(&ShmPTR->mutex);
      shmdt((void *) ShmPTR);
      shmctl(ShmID, IPC_RMID, NULL);
      return 1;
     }

     printf("Server is about to fork %ld child process(es)...\n", studentCount);

     /* Create the requested number of Student processes */
     for (i = 0; i < studentCount; i++) {
      pid = fork ();

      if (pid < 0) {
        perror("fork");

        /* Stop and wait for any Students already created */
        for (int j = 0; j < i; j++) {
          kill(childPIDs[j], SIGTERM);
        }

        for (int j = 0; j < i; j++) {
          waitpid(childPIDs[j], &status, 0);
        }

        /* Terminate and wait for Mom if she was created */
        if (momPID > 0) {
          kill(momPID, SIGTERM);
          waitpid(momPID, &status, 0);
        }

        free(childPIDs);
        sem_destroy(&ShmPTR->mutex);
        shmdt((void *) ShmPTR);
        shmctl(ShmID, IPC_RMID, NULL); 
        return 1;
      }
      else if (pid == 0) {
        ClientProcess(ShmPTR, i + 1);
        exit(0);
      }
      else {
        childPIDs[i] = pid;
      }
     }

     /* Run Dear Old Dad in the parent process */
     DadProcess(ShmPTR);

     /* Terminate all Student processes */
     for (i = 0; i < studentCount; i++) {
      kill(childPIDs[i], SIGTERM);
     }

     /* Terminate Mom if she was created */
     if (momPID > 0) {
      kill(momPID, SIGTERM);
     }

     /* Wait for all Student processes to finish */
     for (i = 0; i < studentCount; i++) {
      waitpid(childPIDs[i], &status, 0);
     }

     /* Wait for Mom if she was created */
     if (momPID > 0) {
      waitpid(momPID, &status, 0);
     }

     printf("Server has detected the completion of its child...\n");

     free(childPIDs);

     sem_destroy(&ShmPTR->mutex);

     shmdt((void *) ShmPTR);
     printf("Server has detached its shared memory...\n");

     shmctl(ShmID, IPC_RMID, NULL);
     printf("Server has removed its shared memory...\n");

     printf("Server exits...\n");
     exit(0);
}

void DadProcess(SharedData *SharedPTR)
{
  int localBalance;
  int amount;
  int randomNumber;

  srand(time(NULL) ^ getpid());

  /* Get the initial bank account balance */
  sem_wait(&SharedPTR->mutex);
  localBalance = SharedPTR->BankAccount;
  sem_post(&SharedPTR->mutex);

  while (!stop) {

    /*Sleep for a random amount of time between 0-5 seconds */
    sleep(rand() % 6);

    if (stop) {
      break;
    }

    printf("Dear Old Dad: Attempting to Check Balance\n");

    /* Generate a random number */
    randomNumber = rand();

    if (randomNumber % 2 == 0) {

      if (localBalance < 100) {

        sem_wait(&SharedPTR->mutex);

        /* Copy shared balance into local balance */
        localBalance = SharedPTR->BankAccount;

        /* Generate a random deposit amount */
        amount = rand() % 100;

        if (amount % 2 == 0) {

          localBalance = localBalance + amount;

          printf("Dear Old Dad: Deposits $%d / Balance = $%d\n", amount, localBalance);
        }
        else { 
          printf("Dear Old Dad: Doesn't have any money to give\n");
        }

        /* Update shared bank account */
        SharedPTR->BankAccount = localBalance;

        sem_post(&SharedPTR->mutex);
      }
      else {
        printf("Dear Old Dad: Thinks Student has enough Cash ($%d)\n", localBalance);
      }
    }
    else {
      printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
    }
  }
}

void MomProcess(SharedData *SharedPTR)
{
  int localBalance;
  int amount;

  srand(time(NULL) ^ getpid());

  while (!stop) {

    /* Sleep for a random amount of time between 0-10 seconds */
    sleep(rand() % 11);

    if (stop) {
      break;
    }

    printf("Loveable Mom: Attempting to Check Balance\n");

    /* Lock the shared bank account */
    sem_wait(&SharedPTR->mutex);

    /* Check the current shared balance */
    localBalance = SharedPTR->BankAccount;

    /* Deposit money if the balance is $100 or less */
    if (localBalance <= 100) {

      /* Generate a random deposit amount between $0 and $125 */
      amount = rand() % 126;

      /* Add the deposit to the local balance */
      localBalance = localBalance + amount;

      printf("Lovable Mom: Deposits $%d / Balance = $%d\n", amount, localBalance);

      /* Update the shared bank account */
      SharedPTR->BankAccount = localBalance;
    }

    sem_post(&SharedPTR->mutex);
  }
}

void  ClientProcess(SharedData *SharedPTR, int studentNumber)
{
  int localBalance;
  int need;
  int randomNumber;

  srand(time(NULL) ^ getpid());

  /* Get the initial bank account balance */
  sem_wait(&SharedPTR->mutex);
  localBalance = SharedPTR->BankAccount;
  sem_post(&SharedPTR->mutex);

  while(!stop) {

    /* Sleep for a random amount of time between 0-5 seconds */
    sleep(rand() % 6);

    if (stop) {
      break;
    }

    printf("Poor Student #%d: Attempting to Check Balance\n", studentNumber);

    /* Generate a random number */
    randomNumber = rand();

    if (randomNumber % 2 == 0) {

      sem_wait(&SharedPTR->mutex);

      /* Copy shared balance into local balance */
      localBalance = SharedPTR->BankAccount;

      /* Generate a random amount of money needed */
      need = rand() % 50;

      printf("Poor Student needs $%d\n", need);

      if (need <= localBalance) {
        
        localBalance = localBalance - need;

        printf("Poor Student #%d: Withdraws $%d / Balance = $%d\n", studentNumber, need, localBalance);
      }
      else {
        printf("Poor Student #%d: Not Enough Cash ($%d)\n", studentNumber, localBalance);
      }

      /* Update shared bank account */
      SharedPTR->BankAccount = localBalance;

      sem_post(&SharedPTR->mutex);
    }
    else{
      printf("Poor Student #%d: Last Checking Balance = $%d\n", studentNumber, localBalance);
    }
  }
}