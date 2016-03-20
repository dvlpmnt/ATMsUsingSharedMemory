#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/msg.h> 
#include <sys/sem.h> 
#include <unistd.h>

struct each
{
	int	account_no;
	int balance;
	char stamp[26];
} ;

typedef struct global 	// struct to be shared globally, contains account details of clients
{
	int size;
	struct each list[500];
} global_details;

//////////////////////////////

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

//////////////////////////////

struct atm
{
	int	atm_key;
	key_t msg_key;
	key_t sem_key;
	key_t shm_key;
} ;

typedef struct loc 		// ATM Locator, to be shared globally
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
locator *atm;
global_details 	*ac_global;
int valid = 1;
int view( int account_no);
void add_account(int account_no);
void set_timestamp(char S[]);
void handler(int i);

int main(int argc, char *argv[])
{
	signal(SIGINT, handler);
	if( argc != 1)
	{
		printf("\n\tWrong arguments!\n");
		return 1;
	}
	int atm_count;
	printf("Enter Number of ATMs :\n");
	scanf("%d",&atm_count);
	int 	shm_ac_id;
	int 	shm_locator_id;
	int 	msg_id;
	key_t 	key_sem = 500;
	key_t 	key_ac_shm = 700;
	key_t 	key_locator_shm = 900;
	key_t 	key_msg = 1025;	//master will interact with ATMs using this msg queue

	int sem_id;
	sem_id = semget(key_sem, 1, IPC_CREAT | 0666);	// 1 subsemaphore for ATM lock
	semctl(sem_id, 0, SETVAL, atm_count);		// Mutex for Locking Queue1

	msg_id = msgget(key_msg,IPC_CREAT|0644);	
	msgctl(msg_id, IPC_RMID, NULL);	// remove already existing queue
	msg_id = msgget(key_msg,IPC_CREAT|0644);	// recreate a queue which is empty
    
	// globally shared memory containing account info
	shm_ac_id = shmget(key_ac_shm, sizeof(global_details), IPC_CREAT|0666);
	ac_global = (global_details *)shmat(shm_ac_id,NULL,0);

	// globally shared memory containing ATM Locator
	shm_locator_id = shmget(key_locator_shm, sizeof(locator), IPC_CREAT|0666);
	atm = (locator *)shmat(shm_locator_id,NULL,0);

	int i, j, k, m;

	// Initialize ATM Locator 
	atm->atm_count = atm_count;
	for(i = 0;i < atm_count; i++)
	{
		atm->list[i].atm_key = i;
		atm->list[i].shm_key = 5000 + i*5;
		atm->list[i].msg_key = 5000 + i*5;
		atm->list[i].sem_key = 5000 + i*5;
	}

	// initialize 'ac_global'
	ac_global->size = 0;

	char temp[100];
	for (i = 0; i < atm_count; i++)
	{
		if (fork() == 0)
		{
			memset(temp,'\0',sizeof(temp));
			sprintf(temp,"./atm %d",i);
			//printf("%s\n",temp);
			execl("/usr/bin/xterm", "/usr/bin/xterm", "-e", "bash", "-c", temp, (void*)NULL);
		}
	}
	memset(msg.mtext,'\0',msglen);
	//printf("msgid %d\n",msg_id);
	while(valid)
	{
		for ( i = 0; i < atm_count && valid == 1; i++)
		{
			if(msgrcv(msg_id, &msg, msglen, 5000 + i, IPC_NOWAIT) != -1)
			{
				printf("ATM%d : Client %s entered\n",i,msg.mtext);
				add_account( atoi( msg.mtext));
				memset(msg.mtext,'\0',msglen);
			}
			if(msgrcv(msg_id, &msg, msglen, 2000 + i, IPC_NOWAIT) != -1)
			{
				printf("ATM%d : Running a consistency check for a/c %s\n", i, msg.mtext);
				j = view( atoi( msg.mtext));
				memset( msg.mtext, '\0', msglen);
				sprintf( msg.mtext, "%d", j);
				msg.mtype = 200 + i;
				printf("ATM%d : Done\n",i);
				while(msgsnd(msg_id,&msg,strlen(msg.mtext),0) == -1);
				memset(msg.mtext,'\0',msglen);
			}
		}
	}
	return 0;
}

void add_account(int account_no)
{
	int n = ac_global->size, i, j;
	for( j = 0, i = 0; i < n && j == 0; i++)
	{
		if( ac_global->list[i].account_no == account_no)
		{
			j = 1;
		}
	}
	if( j == 0)
	{
		return ;
	}
	ac_global->list[n].account_no = account_no;
	ac_global->list[n].balance = 0;
	set_timestamp( ac_global->list[n].stamp);
	ac_global->size = n + 1;
}

int view( int account_no)
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
					sum += ac_temp->list_ac[j].list_trans[k].amount * ac_temp->list_ac[j].list_trans[k].type;
				}
			}
		}
		shmdt((char *)ac_temp);
	}
	return sum;
}

void set_timestamp(char S[])
{
	time_t timer;
	struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    memset(S,'\0',sizeof(S));
    strftime(S, sizeof(S), "%Y/%m/%d %H:%M:%S", tm_info);
}

void handler(int i)
{
	shmdt((char *) ac_global);
	shmdt((char *) atm);
	valid = 0;
	exit(1);
}
