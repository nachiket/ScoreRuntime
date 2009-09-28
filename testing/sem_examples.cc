#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <cstdlib>
#include <stdio.h>
#include <pthread.h>

// Example from O'Reilly's website... suitably modified to work with g++

using namespace std;
//create user defined semun for initializing the semaphores

void *Thread1(void* arg)
{

	int semid=(int) arg;
	//in order to perform the operations on semaphore
	// first need to define the sembuf object
	struct sembuf op1,op2;

	//operation for 0th semaphore
	op1.sem_num = 0; //signifies 0th semaphore
	op1.sem_op = -1; //reduce the semaphore count to lock
	op1.sem_flg = 0; //wait till we get lock on semaphore

	//operation for 1th semaphore
	op2.sem_num = 1; //signifies 0th semaphore
	op2.sem_op = -1; //reduce the semaphore count to lock
	op2.sem_flg = 0; //wait till we get lock on semaphore

	//locking the 0th semaphore
	if (semop(semid,&op1,1) == -1)
	{
		perror("Thread1:semop failure Reason:");
		exit(-1);
	}
	else
		fprintf(stderr,"Thread1:Successfully locked 0th semaphore\n");
	//lock the 1th semaphore
	if (semop(semid,&op2,1) == -1)
	{
		perror("Thread1:semop failure Reason:");
		exit(-1);
	}
	else
		fprintf(stderr,"Thread1:Successfully locked 1th semaphore\n");



	//release the 0th semaphore
	op1.sem_num = 0; //signifies 0th semaphore
	op1.sem_op = 1; //reduce the semaphore count to lock
	op1.sem_flg = 0; //wait till we get lock on semaphore

	if (semop(semid,&op1,1) == -1)
	{
		perror("Thread1:semop failure Reason:");
		exit(-1);
	}
	else
		fprintf(stderr,"Thread1:Successfully unlocked 0th semaphore\n");

	//release the 1th semaphore
	op2.sem_num = 1; //signifies 0th semaphore
	op2.sem_op = 1; //reduce the semaphore count to lock
	op2.sem_flg = 0; //wait till we get lock on semaphore

	if (semop(semid,&op2,1) == -1)
	{
		perror("Thread1:semop failure Reason:");
		exit(-1);
	}
	else
		fprintf(stderr,"Thread1:Successfully unlocked 1th semaphore\n");
}

void *Thread2(void* arg) {

	int semid=(int) arg;
	//in order to perform the operations on semaphore
	// first need to define the sembuf object
	struct sembuf op1,op2;

	//operation for 0th semaphore
	op1.sem_num = 0; //signifies 0th semaphore
	op1.sem_op = -1; //reduce the semaphore count to lock
	op1.sem_flg = 0; //wait till we get lock on semaphore

	//operation for 1th semaphore
	op2.sem_num = 1; //signifies 0th semaphore
	op2.sem_op = -1; //reduce the semaphore count to lock
	op2.sem_flg = 0; //wait till we get lock on semaphore

	//lock the 0th semaphore
	if (semop(semid,&op1,1) == -1)
	{
		perror("Reason:");
		exit(-1);
	}
	else
		fprintf(stderr,"Thread2:Successfully locked 0th semaphore\n");

	//lock the 1th semaphore
	if (semop(semid,&op2,1) == -1)
	{
		perror("Reason:");
		exit(-1);
	}
	else
		fprintf(stderr,"Thread2:Successfully locked 1th semaphore\n");

	//release 0th semaphore

	op1.sem_num = 0; //signifies 0th semaphore
	op1.sem_op = 1; //reduce the semaphore count to lock
	op1.sem_flg = 0; //wait till we get lock on semaphore

	if (semop(semid,&op1,1) == -1)
	{
		perror("Reason:");
		exit(-1);
	}
	else
		fprintf(stderr,"Thread2:Successfully unlocked 0th semaphore\n");

	//release the 1th semaphore
	op2.sem_num = 1; //signifies 0th semaphore
	op2.sem_op = 1; //reduce the semaphore count to lock
	op2.sem_flg = 0; //wait till we get lock on semaphore

	if (semop(semid,&op2,1) == -1)
	{
		perror("Reason:");
		exit(-1);
	}
	else
		fprintf(stderr,"Thread2:Successfully unlocked 1th semaphore\n");

}

int main()
{
	pthread_t tid1,tid2;
	int semid;

	//create user defined semun for initializing the semaphores

	typedef union semun
	{
		int val;
		struct semid_ds *buf;
		ushort * array;
	}semun_t;

	semun_t arg;
	semun_t arg1;

	//creating semaphore object with two semaphore in a set
	//viz 0th & 1th semaphore
	semid = semget(IPC_PRIVATE,2,0666|IPC_CREAT);
	if(semid<0)
	{
		perror("semget failed Reason:");
		exit(-1);
	}

	//initialize 0th semaphore in the set to value 1
	arg.val = 1;
	if ( semctl(semid,0,SETVAL,arg)<0 )
	{
		perror("semctl failure Reason:");
		exit(-1);
	}
	//initialize 1th semaphore in the set to value 1
	arg1.val = 1;
	if( semctl(semid,1,SETVAL,arg1)<0 )
	{
		perror("semctl failure Reason: ");
		exit(-1);
	}

	//create two threads to work on these semaphores
	if(pthread_create(&tid1, NULL,Thread1, (void*)semid))
	{
		printf("\n ERROR creating thread 1");
		exit(1);
	}
	if(pthread_create(&tid2, NULL,Thread2, (void*) semid) )
	{
		printf("\n ERROR creating thread 2");
		exit(1);
	}
	//waiting on these threads to complete
	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);

	//once done clear the semaphore set
	if (semctl(semid, 1, IPC_RMID ) == -1 )
	{
		perror("semctl failure while clearing Reason:");
		exit(-1);
	}
	//exit the main threads
	pthread_exit(NULL);
	return 0;
}
