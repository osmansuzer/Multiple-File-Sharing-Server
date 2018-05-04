
/*


		Osman Suzer 131044051

		fileShareServer.c


*/


#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h> 
#include <signal.h>


#define MAX_CLIENT 100

#define REQ_LS_CLIENTS 111
#define REQ_SEND_FILE 112
#define REQ_LIST_SERVER 113
#define REQ_KILL_ME 114


#define CON_LIVE 1
#define CON_END 0
#define CON_DIE -1
#define CON_NON -11

#define TO_SERVER -111

#define SERV_TAKE_FILE 211
#define SERV_LIVE 212
#define SERV_DIE 213
#define SERV_NO_CLIENT 214

typedef struct
{
    int condition;

    int client_id;
    int connfd;
    pthread_mutex_t mutex;

}client;

typedef struct
{
    int client_id;

}connect_message;

typedef struct
{
    int sockfd;
    int client_id;

}for_thread;



typedef struct
{
    char filename[30];
    struct stat file_property;

    int giver_client_id;
    int taker_client_id;

}client_message;



void *take_file(void *structure);
void *send_file(void *structure);
void *client_thread(void *structure);
void *send_clients(void *structure);
void *sendLocal(void *structure);

int give_client_connfd(int client_id);
int give_client_id(int connfd);

int WordSize(const char Word[]);

void SignalHandler(int signal);

client all_clients[MAX_CLIENT];

int main(int argc, char const *argv[])
{

	int listenfd = 0;
    int connfd = 0;
    struct sockaddr_in serv_addr;
    unsigned portNumber;

    signal(SIGINT, SignalHandler);

    all_clients[0].condition = CON_END;
    int i=0;
    for(i=0; i<MAX_CLIENT; ++i)
    {
    	all_clients[i].condition == CON_NON;
    }


	/*argument's check*/
	if(argc != 2)
	{
		printf("Wrong argument!\nPlease enter only a port number.\nfileShareServer <port number>\n");
		exit(1);
	}
	/*assign the argument to (int)portNumber*/
	portNumber = atoi(argv[1]);
	/*end of argument's check*/


    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    printf("Socket retrieve success\n");

    memset(&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portNumber);

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in));

    if(listen(listenfd, 10) == -1)
    {
        printf("Failed to listen\n");
        return -1;
    }

    printf("-------------------------------------------------\n");
    printf("SERVER PID : %d\n", getpid());
    printf("-------------------------------------------------\n");

    while(1)
    {
        /****************************/
        connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL);

        /************************************************/
        int client_id = -1;

        client_id = give_client_id(connfd);


        if(client_id == -1)
        {
            printf("Error : give client id...\n");
        }
        /************************************************/

        for_thread structure;
        structure.sockfd = connfd;
        structure.client_id = client_id;


        pthread_t tid;
        
        pthread_attr_t attr;
        
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        
        pthread_create(&tid, &attr, client_thread, &structure);
   
    }


    for(i=0; i<MAX_CLIENT; ++i)
    {
        if(all_clients[i].condition == CON_LIVE || 
            all_clients[i].condition == CON_DIE)
        {
            close(all_clients[i].connfd);
            pthread_mutex_destroy(&all_clients[i].mutex);
        }
    }
	

	return 0;
}

void *client_thread(void *structure)
{
    for_thread *temp = (for_thread *)structure;

    connect_message conn_msg;
    conn_msg.client_id = temp->client_id;

    int client_id = temp->client_id;
    int connfd = temp->sockfd;


    printf("------------------------------------\n");
    printf("Connect a client > client ID : %d\n", client_id);
    printf("------------------------------------\n");

    /*write to client a connect message(client id)*/
    write(connfd, &conn_msg, sizeof(connect_message));

    while(1)
    {
        int msg;

       	if(read(connfd, &msg, sizeof(int)) == -1)
        {
            
            kill_client(client_id);
            printf("------------------------------------\n");
            printf("Disconnect a client > client ID : %d\n", client_id);
            printf("------------------------------------\n");
            break;
        }

        if(msg == REQ_LS_CLIENTS)
        {

            for_thread structure;
            structure.client_id = client_id;
            structure.sockfd = connfd;

            send_clients(&structure);
        }
        if(msg == REQ_SEND_FILE)
        {
            for_thread structure;
            structure.sockfd = connfd;
            structure.client_id = client_id;

            /*--------joinable thread yap-------------*/
            take_file(&structure);

        }
        if(msg == REQ_LIST_SERVER)
        {
            for_thread structure;
            structure.sockfd = connfd;
            structure.client_id = client_id;

            sendLocal(&structure);

        }
        if(msg == REQ_KILL_ME)
        {
            kill_client(client_id);
            printf("------------------------------------\n");
            printf("Disconnect a client > client ID : %d\n", client_id);
            printf("------------------------------------\n");
            break;
        }

    }

    close(connfd);

    pthread_detach(pthread_self());

    return 0;

}

void *take_file(void *structure)
{
    sigset_t sset;
    sigemptyset(&sset);
    sigaddset(&sset, SIGINT);

    sigprocmask(SIG_BLOCK, &sset, NULL);

    for_thread *temp = (for_thread *)structure;

    int connfd = temp->sockfd;
    int client_id = temp->client_id;

    pthread_mutex_lock(&all_clients[client_id-1].mutex);

    client_message msg;

    read(connfd, &msg, sizeof(client_message));
    int size_of_file = (int)msg.file_property.st_size;


    if(msg.taker_client_id != TO_SERVER)
    {
    	printf("------------------------------------------------------\n");
    	printf("Client %d requested to send file (%s) to Client %d.\n", 
    			msg.giver_client_id, msg.filename, msg.taker_client_id);
    	printf("------------------------------------------------------\n");
  	}
  	else
  	{
  		printf("------------------------------------------------------\n");
    	printf("Client %d requested to send file (%s) to Server.\n", 
    			msg.giver_client_id, msg.filename);
    	printf("------------------------------------------------------\n");
  	}

    /*to send server*/
    if(msg.taker_client_id == TO_SERVER)
    {
    	int fd = open(msg.filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);

    	char byte;
    	int i=0;
    	for(i=0; i<size_of_file; ++i)
   		{
        	read(connfd, &byte, 1);
        	write(fd, &byte, 1);
    	}

    	printf("------------------------------------------------------\n");
    	printf("File (%s) was taken to Server from Client %d.\n", 
    		msg.filename, msg.giver_client_id);
    	printf("------------------------------------------------------\n");

    	close(fd);
    }
    /*if client id is not correct*/
    else if(msg.taker_client_id <= 0 && msg.taker_client_id >= MAX_CLIENT)
    {
    	char byte;
    	int i=0;
    	for(i=0; i<size_of_file; ++i)
   		{
        	read(connfd, &byte, 1);
    	}

    	int req = SERV_NO_CLIENT;
    	int id = msg.taker_client_id;
    	write(connfd, &req, sizeof(int));
    	write(connfd, &id, sizeof(int));

    	printf("------------------------------------------------------\n");
    	printf("File (%s) wasn't sent to Client %d from Client %d.\n", 
    		msg.filename, msg.taker_client_id, msg.giver_client_id);
    	printf("Because of Client id (%d) is not corrent.\n", msg.taker_client_id);
    	printf("------------------------------------------------------\n");

    }
    /*take and send file if taker client is exist*/
    else if(all_clients[msg.taker_client_id-1].condition == CON_LIVE)
    {
		pthread_mutex_lock(&all_clients[msg.taker_client_id-1].mutex);

    	int req = SERV_TAKE_FILE;
    	write(all_clients[msg.taker_client_id-1].connfd, &req, sizeof(int));

    	write(all_clients[msg.taker_client_id-1].connfd, &msg, sizeof(client_message));

		char byte;
		int i=0;
		for(i=0; i<size_of_file; ++i)
		{	
			read(connfd, &byte, 1);
			write(all_clients[msg.taker_client_id-1].connfd, &byte, 1);
		}

		printf("------------------------------------------------------\n");
    	printf("File (%s) was sent to Client %d from Client %d.\n", 
    		msg.filename, msg.taker_client_id, msg.giver_client_id);
    	printf("------------------------------------------------------\n");

    	pthread_mutex_unlock(&all_clients[msg.taker_client_id-1].mutex);

    }
    /*if taker client is not exist*/
    else
    {

    	char byte;
    	int i=0;
    	for(i=0; i<size_of_file; ++i)
   		{
        	read(connfd, &byte, 1);
    	}
		
    	int req = SERV_NO_CLIENT;
    	int id = msg.taker_client_id;
    	write(connfd, &req, sizeof(int));
    	write(connfd, &id, sizeof(int));

    	printf("------------------------------------------------------\n");
    	printf("File (%s) wasn't sent to Client %d from Client %d.\n", 
    		msg.filename, msg.taker_client_id, msg.giver_client_id);
    	printf("Because of Client %d is not connected now.\n", msg.taker_client_id);
    	printf("------------------------------------------------------\n");

	}

    pthread_mutex_unlock(&all_clients[client_id-1].mutex);

    pthread_detach(pthread_self());

    sigprocmask(SIG_UNBLOCK, &sset, NULL);

    return 0;

}


void *send_clients(void *structure)
{

    sigset_t sset;
    sigemptyset(&sset);
    sigaddset(&sset, SIGINT);

    sigprocmask(SIG_BLOCK, &sset, NULL);

    for_thread *temp = (for_thread *)structure;

    int connfd = temp->sockfd;
    int client_id = temp->client_id;
    
    pthread_mutex_lock(&all_clients[client_id-1].mutex);

    int request = REQ_LS_CLIENTS;
    write(connfd, &request, sizeof(int));

    int i=0;

    for(i=0; i<MAX_CLIENT; ++i)
    {
        if(all_clients[i].condition == CON_END)
            break;
        else if(all_clients[i].condition == CON_LIVE)
        {
            int id = all_clients[i].client_id;
            write(connfd, &id, sizeof(int));
        }
    }
    
    int end = -1;
    write(connfd, &end , sizeof(int));

    pthread_mutex_unlock(&all_clients[client_id-1].mutex);

    pthread_detach(pthread_self());

    sigprocmask(SIG_UNBLOCK, &sset, NULL);

    return 0;
    
}

void *sendLocal(void *structure)
{
    sigset_t sset;
    sigemptyset(&sset);
    sigaddset(&sset, SIGINT);

    sigprocmask(SIG_BLOCK, &sset, NULL);

    for_thread *temp = (for_thread *)structure;
    int connfd = temp->sockfd;
    int client_id = temp->client_id;

    pthread_mutex_lock(&all_clients[client_id-1].mutex);

    int request = REQ_LIST_SERVER;
    write(connfd, &request, sizeof(int));

    char fileName[256];
    getcwd(fileName, 256);

    DIR * directory;
    struct dirent *dir;

    directory = opendir(fileName);

    if(directory == NULL)
    {
        printf ("Wrong Directory.\n");
    }

    int i=0;

    /*while directory has file.*/
    while( (dir = readdir(directory)) != NULL) 
    {
            /*control where file is.*/
            if(strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 )
            {
                char *dName = dir->d_name;
                int wordSize = WordSize(dName);
                if(dName[wordSize-1] != '~')
                {
                    
                    char tab = '\t';
                    write(connfd, &tab, sizeof(char));
                    write(connfd, &tab, sizeof(char));
                    int j=0;
                    for(;j<wordSize; ++j)
                    {
                        write(connfd, &dName[j], sizeof(char));
                       
                    }
                    char enter = '\n';
                    write(connfd, &enter, sizeof(char));
                   
                }
            }
    }

    char n = '\0';
    write(connfd, &n, sizeof(char));

    pthread_mutex_unlock(&all_clients[client_id-1].mutex);


    

    pthread_detach(pthread_self());
    sigprocmask(SIG_UNBLOCK, &sset, NULL);
    return 0;

}


/*yeniden gözden geçir...*/
int give_client_id(int connfd)
{

    int i=0;
    for(i=0; i < MAX_CLIENT; ++i)
    {
        if(all_clients[i].condition == CON_END && i<MAX_CLIENT)
        {
           
            all_clients[i].condition = CON_LIVE;
            all_clients[i].client_id = i+1;
            all_clients[i].connfd = connfd;

            pthread_mutex_init(&all_clients[i].mutex, NULL);
            all_clients[i+1].condition = CON_END;

            return i+1;
            
        }
    }

    for(i=0; i < MAX_CLIENT; ++i)
    {
        if(all_clients[i].condition == CON_DIE)
        {
            all_clients[i].condition = CON_LIVE;
            all_clients[i].client_id = i+1;
            all_clients[i].connfd = connfd;

            pthread_mutex_init(&all_clients[i].mutex, NULL);

            return i+1;
        }
    }

    return -1;
}

int give_client_connfd(int client_id)
{
	int i=0;
	for(i=0; i<MAX_CLIENT; ++i)
	{
		if(all_clients[i].condition == CON_LIVE && 
			all_clients[i].client_id == client_id)
		{
			return i;
		}
		else if(all_clients[i].condition == CON_END)
		{
			return -1;
		}
	}

	return -1;
}


/* return an integer.
integer is size of Word.*/
int WordSize(const char Word[])
{
    int count =0;

    for(; Word[count]!='\0'; ++count)
    {}
    return count;
}

int kill_client(int client_id)
{
    if(all_clients[client_id-1].condition == CON_LIVE)
    {
        all_clients[client_id-1].condition = CON_DIE;
        return CON_DIE;
    }

    return -2;
}

void SignalHandler(int signal)
{
	if(signal == SIGINT)
	{
		int i=0;
		for(i=0; i<MAX_CLIENT; ++i)
		{
			if(all_clients[i].condition == CON_LIVE)
			{
				int msg = SERV_DIE;
				write(all_clients[i].connfd, &msg, sizeof(int));
			}
		}
		for(i=0; i<MAX_CLIENT; ++i)
    	{
        	if(all_clients[i].condition == CON_LIVE || 
            	all_clients[i].condition == CON_DIE)
       		{
            	close(all_clients[i].connfd);
            	pthread_mutex_destroy(&all_clients[i].mutex);
        	}
    	}
		
		exit(1);
	}

}