/*

		Osman Suzer 131044051

		client.c

*/

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <dirent.h> 
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define REQ_LS_CLIENTS 111
#define REQ_SEND_FILE 112
#define REQ_LIST_SERVER 113
#define REQ_KILL_ME 114

#define TO_SERVER -111

#define SERV_TAKE_FILE 211
#define SERV_LIVE 212
#define SERV_DIE 213
#define SERV_NO_CLIENT 214

typedef struct
{
	int sockfd;

}for_thread;

typedef struct
{
	int sockfd;
	char filename[30];
	int taker_client_id;
}for_send_file;

typedef struct
{
    char filename[30];
    struct stat file_property;

    int giver_client_id;
    int taker_client_id;

}client_message;

typedef struct
{
    int client_id;

}connect_message;

void *send_file(void *structure);
void listLocal();
void help();
void *lsClients(void *structure);
void *listServer(void *structure);

void *read_from_server(void *structure);
void *take_file(void *structure);

void SignalHandler(int signal);

int is_there_in_local(char file_name[256]);
void give_command(char com[45], char command[15], char file[30], int *taker_client_id);

int WordSize(const char Word[]);

pthread_mutex_t mutex;

static unsigned PORT_NUMBER = 123456;
int MY_ID;
int MY_SOCKED;

pthread_t send_thread;
pthread_t take_thread;

pthread_t read_thread;

int take_flag = 0;
int send_flag = 0;

int main(int argc, char const *argv[])
{
    int sockfd = 0;
    struct sockaddr_in serv_addr;

    signal(SIGINT, SignalHandler);

    pthread_mutex_init(&mutex, NULL);

    if(argc != 3)
    {
    	printf("Wrong argument!\n");
    	printf("Please enter IP Address of Server and Port number.\n");
    	printf("./server <IP Address> <Port Number>\n");
    }

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0))< 0)
    {
        printf("\n Error : Could not create socket \n");
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    int port;
   	port = atoi(argv[2]);
    serv_addr.sin_port = htons(port);

    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
    
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0)
    {
        printf("\nError : Connect Failed!\n");
        close(sockfd);
        return 1;
    }

    printf("-----------------------------------------\n");
    printf("PID : %d\n", getpid());

    connect_message conn_msg;
	read(sockfd, &conn_msg, sizeof(connect_message));

	MY_ID = conn_msg.client_id;
	MY_SOCKED = sockfd;

	
	printf("CLIENT ID : %d\n", conn_msg.client_id);
	printf("-----------------------------------------\n");


	for_thread structure;
   	structure.sockfd = sockfd;

    pthread_create(&read_thread, NULL, read_from_server, &structure);

	
	char command[50];
	char *taker_c;
	char *file;

	printf("\nEnter a command > ");
	fgets(command, 50, stdin);

	strtok(command, " ");
	file = strtok(NULL, " ");
	taker_c = strtok(NULL, " ");

	
	while(1)
	{

		if(strcmp(command, "listLocal\n") == 0)
		{
			listLocal();
		}
		else if(strcmp(command, "listServer\n") == 0)
		{
    		int request = REQ_LIST_SERVER;
			write(sockfd, &request, sizeof(int));
			sleep(1);
		}
		else if(strcmp(command, "lsClients\n") == 0)
		{
    		int request = REQ_LS_CLIENTS;
			write(sockfd, &request, sizeof(int));
			sleep(1);
		}
		else if(strcmp(command, "sendFile") == 0)
		{

			for_send_file str;
			str.sockfd = sockfd;

			
			
			if(taker_c == NULL)
			{
				str.taker_client_id = TO_SERVER;
				strcpy(str.filename, file);
				str.filename[WordSize(str.filename)-1] = '\0';
			}
			else
			{
				str.taker_client_id = atoi(taker_c);
				strcpy(str.filename, file);
			}


    		if(is_there_in_local(str.filename) < 0)
    		{
    			printf("\t!Wrong file name : %s\n", str.filename);
    		}
    		else if(str.taker_client_id == MY_ID)
			{
				printf("\t!%d : this id is yours.\n", str.taker_client_id);
			}
    		else
    		{
    			pthread_create(&send_thread, NULL, send_file, &str);
    			sleep(1);
    		}
		}
		else if(strcmp(command, "help\n") == 0)
		{
			help();
		}
		else
		{
			printf("Wrong command!\nPlease enter again a command > ");
		}

		pthread_mutex_lock(&mutex);

		/*
		printf("\nEnter a command > ");
		fgets(command, 100, stdin);
		strtok(command, " ");
		
		filename = strtok(NULL, " ");
		taker_c = strtok(NULL, " ");
		*/
		printf("\nEnter a command > ");
		fgets(command, 50, stdin);

		strtok(command, " ");
		file = strtok(NULL, " ");
		taker_c = strtok(NULL, " ");
	//	scanf("%s", command);

		pthread_mutex_unlock(&mutex);
	}

    close(sockfd);


    return 0;
}


void *send_file(void *structure)
{
	send_flag = 1;

	for_send_file *temp = (for_send_file *)structure;
	int request;
	client_message msg;

	struct stat file_property;
	int size_of_file;

	char byte = 'b';

	sigset_t sset;
	sigemptyset(&sset);
	sigaddset(&sset, SIGINT);

	sigprocmask(SIG_BLOCK, &sset, NULL);

//	memset(&msg, 0, sizeof(client_message));

	/*blocked signals*/

	pthread_mutex_lock(&mutex);

	request = REQ_SEND_FILE;


	write(temp->sockfd, &request, sizeof(int));

	msg.giver_client_id = MY_ID;
	msg.taker_client_id = temp->taker_client_id;

	strcpy(msg.filename, temp->filename);
	
	stat(temp->filename, &file_property);
	size_of_file = (int)file_property.st_size;

	msg.file_property = file_property;

	printf("-----------------------------------------------------\n");
	printf("\tSEND FILE -->\n");
	printf("-----------------------------------------------------\n");

	printf("sending is started...\n");
    printf("------------------------------------------------------\n");
	printf("\n\tfile name: %s\n", msg.filename);
    printf("\tsize of file: %d byte\n", size_of_file);
    printf("\tgiver client : %d\n", msg.giver_client_id);

    if(msg.taker_client_id == TO_SERVER)
    	printf("\ttaker : SERVER\n");
    else
    	printf("\ttaker client : %d\n", msg.taker_client_id);
    printf("------------------------------------------------------\n");

	write(temp->sockfd, &msg, sizeof(client_message));

	int fd = open(temp->filename, O_RDONLY);

	int i=0;
	for(i=0; i<size_of_file; ++i)
	{	
		read(fd, &byte, 1);
		write(temp->sockfd, &byte, 1);
	}

	close(fd);

	printf("\n...sending is finished\n");
	printf("-----------------------------------------------------\n");
	pthread_mutex_unlock(&mutex);

	send_flag = 0;
	/*unblocked signals*/
	sigprocmask(SIG_UNBLOCK, &sset, NULL);

	return 0;
}

void *read_from_server(void *structure)
{
	/*blocked signals*/
	
    for_thread *temp = (for_thread *)structure;

    int connfd = temp->sockfd;

    while(1)
    {
    	int req = -1;

    //	pthread_mutex_lock(&mutex); 

    	read(connfd, &req, sizeof(int));

    	if(req == SERV_DIE)
    	{
    		printf("!!!SERVER is not working...\n");
    		kill(getpid(), SIGINT);
    		break;
    	}
    	else if(req == SERV_TAKE_FILE)
    	{
    		pthread_create(&take_thread, NULL, take_file, structure);
    		//take_file(structure);
    		pthread_join(take_thread, NULL);

    	}
    	else if(req == REQ_LIST_SERVER)
    	{
    		listServer(structure);
    	}
    	else if(req == REQ_LS_CLIENTS)
    	{
    		lsClients(&connfd);

    	}
    	else if(req == SERV_NO_CLIENT)
    	{
    		int id = -1;
    		read(connfd, &id, sizeof(int));
    		printf("Sending is unsuccessful because of no Cliend %d\n", id);
    	}

    //	pthread_mutex_unlock(&mutex); 

    }

    return 0;

}

void *take_file(void *structure)
{
	take_flag = 1;

	for_thread *temp = (for_thread *)structure;
	int connfd = temp->sockfd;

	sigset_t sset;
	sigemptyset(&sset);
	sigaddset(&sset, SIGINT);

	sigprocmask(SIG_BLOCK, &sset, NULL);

	pthread_mutex_lock(&mutex);

	printf("\n-----------------------------------------------------\n");
	printf("\tTAKE FILE -->\n");
	printf("-----------------------------------------------------\n");

	printf("\nPlease wait... Taking (a file) is started...\n");
    client_message msg;

    read(connfd, &msg, sizeof(client_message));
    int size_of_file = (int)msg.file_property.st_size;

    printf("------------------------------------------------------\n");
	printf("\n\tfile name: %s\n", msg.filename);
    printf("\tsize of file: %d byte\n", size_of_file);
    printf("\tgiver client : %d\n", msg.giver_client_id);
    printf("\ttaker client : %d\n", msg.taker_client_id);
    printf("------------------------------------------------------\n");

    /*take file*/

    
   int fd = open(msg.filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);

	char byte = 'b';
	int i=0;
	for(i=0; i<size_of_file; ++i)
	{
    	read(connfd, &byte, 1);
    	write(fd, &byte, 1);
	}

	close(fd);
    
    printf("\n...taking (the file) is finished.\n");
    printf("------------------------------------------------------\n");
    printf("\nEnter a command > ");
    pthread_mutex_unlock(&mutex);
    
    take_flag = 0;

    sigprocmask(SIG_UNBLOCK, &sset, NULL);

    return 0;
}

void listLocal()
{
	char fileName[256];
	getcwd(fileName, 256);

	DIR * directory;
	struct dirent *dir;

	directory = opendir(fileName);

	printf("-----------------------------------------------------\n");
	printf("\tLIST LOCAL -->\n");
	printf("-----------------------------------------------------\n");

	/*while directory has file.*/
	while( (dir = readdir(directory)) != NULL) 
	{
			/*control where file is.*/
			if(strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 )
			{
				char *dName = dir->d_name;
				if(dName[WordSize(dName)-1] != '~')
				{
					printf("\t\t%s\n", dir->d_name);
				}
			}
	}

	printf("-----------------------------------------------------\n");
	closedir(directory);


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

void help()
{
	printf("-----------------------------------------------------\n");
	printf("\tHELP -->\n");
	printf("-----------------------------------------------------\n\n");
	printf("listLocal:\n\tto list the local files in");
		printf("\n\tthe directory client program started\n\n");
	printf("listServer:\n\tto list the files");
		printf("\n\tin the current scope of the server-client\n\n");
	printf("lsClients:\n\tlists the clients currently connected to the server");
		printf("\n\twith their respective clientids\n\n");
	printf("sendFile <filename> <clientid> :");
		printf("\n\tsend the file <filename> (if file exists) from");
		printf("\n\tlocal directory to the client with client id clientid.");
		printf("\n\tIf no client id is given the file is send");
		printf("\n\tto the servers local directory.\n\n");
	printf("help:\n\tdisplays the available comments and their usage\n");
	printf("-----------------------------------------------------\n");
	
}

void *lsClients(void *structure)
{
	sigset_t sset;
	sigemptyset(&sset);
	sigaddset(&sset, SIGINT);

	sigprocmask(SIG_BLOCK, &sset, NULL);

	/*blocked signals*/

	int *sockfd = (int *)structure;

	pthread_mutex_lock(&mutex); 

	printf("-----------------------------------------------------\n");
	printf("\tLSCLIENTS -->\n");
	printf("-----------------------------------------------------\n");


	int other_client_id = 0;

	read(*sockfd, &other_client_id, sizeof(int));

	while(other_client_id != -1)
	{
		printf("\t\tClient ID : %d\n", other_client_id);
		read(*sockfd, &other_client_id, sizeof(int));
	}

	printf("-----------------------------------------------------\n");

	pthread_mutex_unlock(&mutex);

	/*unblocked signals*/
	sigprocmask(SIG_UNBLOCK, &sset, NULL);

	return 0;
}

void *listServer(void *structure)
{

	sigset_t sset;
	sigemptyset(&sset);
	sigaddset(&sset, SIGINT);

	sigprocmask(SIG_BLOCK, &sset, NULL);

	/*blocked signals*/

	int *sockfd = (int *)structure; 

	pthread_mutex_lock(&mutex);

	char file_char = '\0';

	printf("-----------------------------------------------------\n");
	printf("\tLIST SERVER -->\n");
	printf("-----------------------------------------------------\n");

	read(*sockfd, &file_char, sizeof(char));
	if(file_char == '\0')
	{
		printf("No file in server.\n");
		return NULL;
	}
	while(file_char != '\0')
	{
		printf("%c", file_char);
		read(*sockfd, &file_char, sizeof(char));	
	}

	printf("-----------------------------------------------------\n");
	pthread_mutex_unlock(&mutex);

	pthread_detach(pthread_self());

	/*unblocked signals*/
	sigprocmask(SIG_UNBLOCK, &sset, NULL);


	return 0;
}

void SignalHandler(int signal)
{
	if(signal == SIGINT)
	{
		if(send_flag == 0)
			pthread_join(send_thread, NULL);
		else
		{
			while(send_flag == 1);
			pthread_join(send_thread, NULL);
		}
		if(take_flag == 0)
			pthread_join(take_thread, NULL);
		else
		{
			while(take_flag == 1);
			pthread_join(take_thread, NULL);
		}

		pthread_join(read_thread, NULL);

		printf("SIGINT signal(CTRL+C)\n");
		int msg = REQ_KILL_ME;
		write(MY_SOCKED, &msg, sizeof(int));

		exit(1);
	}

}

int is_there_in_local(char file_name[256])
{
	int result = -1;

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
				if(strcmp(dName, file_name) == 0)
				{
					result = 0;
				}
			}
	}

	closedir(directory);
	return result;
}