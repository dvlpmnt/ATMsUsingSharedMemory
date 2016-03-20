#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/msg.h> 
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>

struct transactions
{
	int amount;
	int type;
	char stamp[26];
};

struct temp	//local info associated with an account
{
	int	account_no;
	int	balance;
	int trans_size;
	struct transactions list_trans[500];
};
typedef struct ac
 {
 	int size;
 	struct temp list_ac[100];
 } account;

struct atm
{
	int	atm_key;
	key_t msg_key;
	key_t sem_key;
	key_t shm_key;
} ;

typedef struct loc 		// ATM Locator, globally shared
{
	struct atm list[500];
	int atm_count;
} locator;

struct message
{
	long mtype;
	char mtext[1000];
} msg;

int msglen = 1000;
int	msg_id;
int atm_id;
int client_ac_no;
int valid = 1;

locator *atm;
account *ac_local;

void first_time(int account_no);
int view(int account_no);
void deposit(int account_no, int amount);
void withdraw(int account_no, int amount);
int check_local_consistency(int account_no, int amount);	// amount: amount to be withdrawn
void set_timestamp(char S[]);
int toint(char S[]);
void handler(int i);

int msg_id_global; // to interact with master.c

int main(int argc, char *argv[])
{
	signal(SIGINT,handler);
	atm_id = toint(argv[1]);
	printf("\t\tATM%d\n\n",atm_id);
	// globally shared memory containing ATM Locator
	int 	shm_locator_id;
	key_t key_locator_shm = 900;
	key_t key_msg;
	key_t key_sem;
	key_t key_shm;
	shm_locator_id = shmget(key_locator_shm, sizeof(locator), IPC_CREAT|0666);
	atm = (locator *)shmat(shm_locator_id,NULL,0);

	key_msg = atm->list[atm_id].msg_key;
	key_sem = atm->list[atm_id].sem_key;
	key_shm = atm->list[atm_id].shm_key;
	
	// printf("%d\t%d\t%d\n",key_msg, key_shm, key_sem);
	msg_id = msgget(key_msg,IPC_CREAT|0644);	
	msgctl(msg_id, IPC_RMID, NULL);	// remove already existing queue
	msg_id = msgget(key_msg,IPC_CREAT|0644);	// recreate a queue which is empty
    
	msg_id_global = msgget((key_t)1025,IPC_CREAT|0644);	

	int sem_id;
	sem_id = semget(key_sem, 1, IPC_CREAT | 0666);	// 1 subsemaphore for ATM lock
	semctl(sem_id, 0, SETVAL, 1);		// Mutex for Locking Queue1
	// printf("here semid %d, val %d\n",sem_id,semctl(sem_id, 0, GETVAL, 0));

	int shm_id;
	shm_id = shmget(key_shm, sizeof(account), IPC_CREAT|0666);
	ac_local = (account *)shmat(shm_id,NULL,0);
	ac_local->size = 0;
	int value, reply, n, i;
	while(valid)
	{
		// printf("waiting to Receive from %d@%d\n",msg_id,400);
		memset(msg.mtext,'\0',msglen);
		msgrcv(msg_id, &msg, msglen, 400, 0) ;	// msg.mtext = #2 400 // this is just an example
		// printf("\n\nATM%d, Received %s\n", atm_id, msg.mtext);
		if(strstr(msg.mtext,"#1") != NULL)
		{
			value = toint(&msg.mtext[3]);
			printf("ATM%d : Client %d is connected now.\n", atm_id, value);
			first_time(value);
			memset(msg.mtext,'\0',msglen);
			strcpy(msg.mtext," ");
		}
		else if(strstr(msg.mtext,"#2") != NULL)
		{
			value = toint(&msg.mtext[3]);
			printf("ATM%d : Client %d has requested for withdrawl for amount %d.\n",atm_id, client_ac_no, value);
			if( check_local_consistency(client_ac_no, value))
			{
				withdraw(client_ac_no, value);
				memset(msg.mtext,'\0',msglen);
				strcpy(msg.mtext,"Withdrawl Succesful.");
			}
			else
			{
				memset(msg.mtext,'\0',msglen);
				strcpy(msg.mtext,"Balance Insufficient!");
				printf("ATM%d : Client %d has insufficient balance.\n", atm_id, client_ac_no);
			}
		}
		else if(strstr(msg.mtext,"#3") != NULL)
		{
			value = toint(&msg.mtext[3]);
			printf("ATM%d : Client %d has requested for Deposition for amount %d.\n", atm_id, client_ac_no, value);
			deposit(client_ac_no, value);			
			memset(msg.mtext,'\0',msglen);
			strcpy(msg.mtext,"Deposition Successful.");
		}
		else if(strstr(msg.mtext,"#4") != NULL)
		{	
			printf("ATM%d : Client %d has requested for VIEW.\n", atm_id, client_ac_no);
			memset(msg.mtext,'\0',msglen);
			sprintf(msg.mtext,"Available balance in your a/c is %d.",view(client_ac_no));
		}
		else if(strstr(msg.mtext,"#5") != NULL)
		{	
			printf("ATM%d : Client %d has left the ATM.\n\nATM%d is available now.\n\n", atm_id, client_ac_no,atm_id);
			memset(msg.mtext,'\0',msglen);
			strcpy(msg.mtext,"Bye!");
		}
		msg.mtype = 600;
		// printf("ATM%d, Sending %s\n", atm_id, msg.mtext);
		while(msgsnd(msg_id,&msg,strlen(msg.mtext),0) ==-1);
	}	
	return 0;
}

int toint(char S[])
{
	int n = strlen(S), i = 0 , j = 0;
	for( ; i < n ;i++)
		j = j*10 + S[i] - '0';
	return j;
}

void set_timestamp(char S[])
{
    time_t timer;
	struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    memset(S,'\0',sizeof(S));
    strftime(S, 26,"%Y-%m-%d %H:%M:%S", tm_info);
    // printf("stamp : %s\n",S);
}

int check_local_consistency(int account_no, int amount)	// amount: amount to be withdrawn
{
	int i, j, k, l, n, m, sum = 0, shm_id;
	key_t shm_key;
	n = atm->atm_count;
	account *ac_temp;
	for(i = 0; i < n ; i++)
	{
		shm_key = atm->list[i].shm_key;
		shm_id = shmget(shm_key, sizeof(account), IPC_CREAT|0666);
		ac_temp = (account *)shmat(shm_id,NULL,0);
		m = ac_temp->size;
		for(j = 0; j < m; j++)
		{
			if(ac_temp->list_ac[j].account_no == account_no)
			{
				l = ac_temp->list_ac[j].trans_size;
				for( sum += ac_temp->list_ac[j].balance, k = 0; k < l; k++)
				{
					// printf("atm%d,account_no %d, transactions amt %d, type %d",i,account_no,ac_temp->list_ac[j].list_trans[k].amount, ac_temp->list_ac[j].list_trans[k].type);
					sum += ac_temp->list_ac[j].list_trans[k].amount * ac_temp->list_ac[j].list_trans[k].type;
				}
			}
		}
		shmdt((char *)ac_temp);
	}
	// printf("total bal in local check_local_consistency %d\n",sum);
	return sum >= amount;
}

void withdraw(int account_no, int amount)
{
	int i , flag = 0, j, k, l, m, n;
	n = ac_local->size;
	for(i = 0; i < n ; i++)
	{
		if(ac_local->list_ac[i].account_no == account_no)
			break ;
	}
	k = ac_local->list_ac[i].trans_size;
	ac_local->list_ac[i].list_trans[k].amount = amount;
	ac_local->list_ac[i].list_trans[k].type = -1; // -1 : withdraw
	set_timestamp( ac_local->list_ac[i].list_trans[k].stamp);
	ac_local->list_ac[i].trans_size = k + 1;
}

void deposit(int account_no, int amount)
{
	int i , k, n;
	n = ac_local->size;
	for(i = 0; i < n ; i++)
	{
		if(ac_local->list_ac[i].account_no == account_no)
			break ;
	}
	// printf(" n = %d, found ac %d at %d\namount to add %d\n",n,account_no,i,amount);
	k = ac_local->list_ac[i].trans_size;
	ac_local->list_ac[i].list_trans[k].amount = amount;
	ac_local->list_ac[i].list_trans[k].type = 1; // 1 : Deposit
	set_timestamp( ac_local->list_ac[i].list_trans[k].stamp);
	ac_local->list_ac[i].trans_size = k + 1;
	// printf("%d, %d, %s\n",ac_local->list_ac[i].list_trans[k].amount,ac_local->list_ac[i].list_trans[k].type,ac_local->list_ac[i].list_trans[k].stamp );	
}

int view(int account_no)
{
	memset(msg.mtext,'\0',sizeof(msg.mtext));
	sprintf(msg.mtext,"%d",account_no);
	// printf("Sending to %d\n",msg_id_global);
	msg.mtype = 2000 + atm_id;
	while(msgsnd(msg_id_global,&msg,strlen(msg.mtext),0) == -1);
	// printf("sent\nreceiving\n");
	memset(msg.mtext,'\0',msglen);
	while(msgrcv(msg_id_global, &msg, msglen, 200 + atm_id, 0) == -1);
	// printf("Received %s\n",msg.mtext);
	int i , m, flag = 0, sum = 0, j, k, n;
	n = ac_local->size;
	for(i = 0; i < n ; i++)
	{
		if(ac_local->list_ac[i].account_no == account_no)
			break ;
	}
	m = ac_local->list_ac[i].trans_size;
	for( j = 0; j < m; j++)
	{
		sum += ac_local->list_ac[i].list_trans[j].amount * ac_local->list_ac[i].list_trans[j].type;
	}
	ac_local->list_ac[i].trans_size = 0;
	ac_local->list_ac[i].balance += sum;
	return toint(msg.mtext);
}

void first_time(int account_no)
{
	// printf("Inside first_time\n");
	client_ac_no = account_no;
	int i , m, sum = 0, k, n;
	n = ac_local->size;
	for(i = 0; i < n ; i++)
	{
		if(ac_local->list_ac[i].account_no == account_no)
			return ;
	}
	// printf("\n\n\n\t\tAdding ac %d\n\n\n",account_no );
	ac_local->list_ac[i].account_no = account_no;
	ac_local->list_ac[i].balance = 0;
	ac_local->list_ac[i].trans_size = 0;
	ac_local->size = n + 1;
	memset(msg.mtext,'\0',sizeof(msg.mtext));
	sprintf(msg.mtext,"%d",account_no);
	msg.mtype = 5000 + atm_id;
	while(msgsnd(msg_id_global,&msg,strlen(msg.mtext),0) == -1);
}

void handler(int i)
{
	shmdt((char *)atm);
	shmdt((char *)ac_local);
	valid = 0;
	exit(1);
}


// mtype 5000 + atmid master : add ac if doesnt exist
// mtype 2000 + atmid : send ac no. for view to master from ATM
// mtype 200 + atmid : recv reply to the above from master
// mtype 400 client to atm
// mtype 600 atm to client