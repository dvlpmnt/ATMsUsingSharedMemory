#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/msg.h> 
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>

struct message
{
	long mtype;
	char mtext[1000];
} msg;

int msglen = 1000;
int	msg_id;

int grab_lock(int number);
void interact(int atm_id);
void release_lock(int number);

int atm_id;

int main(int argc, char *argv[])
{
	if( argc != 1)
	{
		printf("Wrong Input\n");
		return 1;
	}

	key_t key_msg;
	key_t key_sem;

    
	int sem_id;
	
	int value, reply, i;
	while(1)
	{
		printf("Client %d : Enter the ATM no. you want to connect to : \n", getpid());
		scanf("%d",&atm_id);
		i = grab_lock(atm_id);
		if(	i == -1)
		{
			printf("ATM%d is currently occupied!\n",atm_id);
			continue;
		}
		else if( i == -2)
		{
			printf("Error : No such ATM exists!\n");
			continue ;
		}
		key_msg = 5000 + atm_id*5;
		msg_id = msgget(key_msg,IPC_CREAT|0644);
		printf("Welcome Client %d\n", getpid());	
		interact(atm_id);
		printf("Good Bye Client %d.\n", getpid());
	}
	return 0;
}

void interact(int atm_id)
{
	//printf("Inside interact to %d\n",msg_id);
	memset(msg.mtext,'\0',sizeof(msg.mtext));
	sprintf(msg.mtext, "#1 %d", getpid());
	msg.mtype = 400;
	while(msgsnd(msg_id,&msg,strlen(msg.mtext),0) == -1);
	//printf("Sent to %d@%ld\n",msg_id,msg.mtype);
	msgrcv(msg_id, &msg, msglen, 600, 0);	// msg.mtext = #2 400 // this is just an example
	//printf("Received\n");
	char S[100];
	int n, i;
	while(1)
	{
		printf("\nClient %d, Enter : \n\t\t1 <amount>: Withdraw\n\t\t2 <amount>: Deposit\n\t\t3 : View\n\t\t4 : Leave\n",getpid());
		scanf("%d",&n);
		if(n > 4 || n < 1)
		{
			printf("Wrong Input!");
			continue;
		}
		memset(msg.mtext,'\0',msglen);
		if( n == 1)
		{
			scanf("%d",&i);
			if( i <= 0)
			{
				printf("Amount must be a positive number!\n");
				continue ;
			}
			sprintf(msg.mtext,"#2 %d", i);
		}
		else if( n == 2)
		{
			scanf("%d",&i);
			if( i <= 0)
			{
				printf("Amount must be a positive number!\n");
				continue ;
			}
			sprintf(msg.mtext,"#3 %d", i);
		}
		else if( n == 3)
		{
			strcpy(msg.mtext,"#4");
		}
		else if( n == 4)
		{
			strcpy(msg.mtext,"#5");
			msg.mtype = 400;
			while(msgsnd(msg_id,&msg,strlen(msg.mtext),0) == -1);
			release_lock(atm_id);
			memset(msg.mtext,'\0',msglen);
			msgrcv(msg_id, &msg, msglen, 600, 0);
			return ;
		}
		else
		{
			printf("Invalid Input!\n");
			continue ;
		}
		msg.mtype = 400;
		while(msgsnd(msg_id,&msg,strlen(msg.mtext),0)==-1);
		memset(msg.mtext,'\0',msglen);
		msgrcv(msg_id, &msg, msglen, 600, 0);
		printf("%s\n",msg.mtext);
	}
}

void release_lock(int number)
{
	key_t 	key_sem;
	int semid;
	key_sem = 5000 + number*5;
	semid = semget(key_sem, 1, IPC_CREAT | 0666);
	struct sembuf sop_var;
	sop_var.sem_num = 0;
	sop_var.sem_op = 1;
	sop_var.sem_flg = 0;
	semop(semid, &sop_var, 1);
}

int grab_lock(int number)
{
	key_t 	key_sem = 500;
	int semid = semget(key_sem, 1, IPC_CREAT | 0666);
	//printf("atm count %d\n", semctl(semid, 0, GETVAL, 0));
	if(number >= semctl(semid, 0, GETVAL, 0) || semctl(semid, 0, GETVAL, 0) == 0)
		return -2 ;
	key_sem = 5000 + number*5;
	semid = semget(key_sem, 1, IPC_CREAT | 0666);
	//printf("here semid %d, val %d\n",semid,semctl(semid, 0, GETVAL, 0));
	if(semctl(semid, 0, GETVAL, 0) <= 0)
		return -1;
	struct sembuf sop_var;
	sop_var.sem_num = 0;
	sop_var.sem_op = -1;
	sop_var.sem_flg = 0;
	semop(semid, &sop_var, 1);
	return 1;
}
