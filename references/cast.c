#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>

#define LOG_DATA 8
#define LOG_DEF LOG_INFO

struct node { //TODO: Remove debugging variables
	uuid_t uuid;
	char ip[INET_ADDRSTRLEN];
	int ch, port, stream;
	time_t lastup;
	pthread_t thread;
	struct sockaddr_in *dgram;
	struct node *next;
} typedef Node, *PNode;

int loglvl;
ssize_t chcount;
PNode *channel;
pthread_rwlock_t *listlock;
pthread_t metathrd, datathrd;
//sem_t dataqueue;

char* timestamp()
{
	static char *ts = NULL;
	if(!ts)
		ts = malloc(20);
	strftime(ts, 20, "%Y-%m-%d %H:%M:%S", localtime(&(time_t){time(NULL)}));
	return ts;
}

char* uuidstring(uuid_t uuid)
{
	static char *str = NULL;
	if(!str)
		str = malloc(37);
	uuid_unparse(uuid, str);
	return str;
}

void printlg(int lvl, const char* fmt, ...)
{
	if(lvl > loglvl)
		return;
	
	va_list ap;
	va_start(ap, fmt);
	printf("%s\t", timestamp());
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
}

int addnode(int ch, PNode node)
{
	if(ch < 0 || ch >= chcount)
	{
		printlg(LOG_WARNING, "Invalid channel to add: %d.", ch);
		return -1;
	}
	
	PNode itr = channel[ch];
	
	pthread_rwlock_wrlock(&listlock[ch]);
	while(itr->next)
		itr = itr->next;
	itr->next = node;
	pthread_rwlock_unlock(&listlock[ch]);
	
	printlg(LOG_INFO, "Added client %s to channel %d.", uuidstring(node->uuid), ch);
	return 0;
}

int remnode(int ch, PNode node)
{
	if(ch < 0 || ch >= chcount)
	{
		printlg(LOG_WARNING, "Invalid channel to remove: %d.", ch);
		return -1;
	}
	
	PNode temp, itr = channel[ch];
	
	pthread_rwlock_wrlock(&listlock[ch]);
	while(itr->next && itr->next != node)
		itr = itr->next;
	if(itr->next)
	{
		temp = itr->next;
		itr->next = itr->next->next;
		temp->next = NULL;
	}
	pthread_rwlock_unlock(&listlock[ch]);
	
	printlg(LOG_INFO, "Removed client %s from channel %d.", uuidstring(node->uuid), ch);
	return 0;
}

PNode findnode(int ch, uuid_t uuid)
{
	if(ch < 0 || ch >= chcount)
	{
		printlg(LOG_WARNING, "Invalid channel to search: %d.", ch);
		return NULL;
	}
	
	pthread_rwlock_rdlock(&listlock[ch]);
	PNode itr = channel[ch]->next;
	
	while(itr)
	{
		//printlg(LOG_DEBUG, "Looking at %s ", uuidstring(itr->uuid));
		//printlg(LOG_DEBUG, "Looking for: %s.", uuidstring(uuid));
		if(!uuid_compare(uuid, itr->uuid))
			break;
		itr = itr->next;
	}
	pthread_rwlock_unlock(&listlock[ch]);
	
	printlg(LOG_DEBUG, itr ? "Found client %s at channel %d."
						   : "Cannot find client %s at channel %d", uuidstring(uuid), ch);
	return itr;
}

int dumpch(int ch) //TODO: Refactor to decrease branching
{
	if(ch < 0 || ch >= chcount)
	{
		printlg(LOG_WARNING, "Invalid channel to dump: %d.", ch);
		return -1;
	}
	
	int i = 0;
	PNode itr = channel[ch];
	printlg(LOG_INFO, "Channel: %d, Index: %d, Entry: %p.", ch, i, itr);
	
	pthread_rwlock_rdlock(&listlock[ch]);
	while(itr->next)
	{
		i++;
		itr = itr->next;
		printlg(LOG_INFO, "Channel: %d, Index: %d, Entry: %s.", ch, i, uuidstring(itr->uuid));
	}
	pthread_rwlock_unlock(&listlock[ch]);
	
	printlg(LOG_INFO, "Channel: %d, Index: %d, Entry: %p", ch, i+1, itr->next);
	return 0;
}

void* cliops(void *param) //TODO: Decrease clustering
{
	int op, data;
	PNode client = (PNode)param;
	
	printlg(LOG_INFO, "Client %s connected at %s %d.", uuidstring(client->uuid), client->ip, client->port);
	addnode(client->ch, client);
	
	while(1)
	{
		if(recv(client->stream, &op, sizeof(int), MSG_WAITALL) != sizeof(int))
			break;
		op = ntohl(op);
		client->lastup = time(NULL);
		if(op == 'h') //Heartbeat signal (Not used anymore, legacy code)
		{
			/*if(send(client->stream, &(int){htonl('h')}, sizeof(int), 0) != sizeof(int))
				break;*/
			printlg(LOG_DATA, "Heartbeat from client %s.", uuidstring(client->uuid));
		}
		if(op == 'c') //Heartbeat - Change channel
		{
			if(recv(client->stream, &data, sizeof(int), MSG_WAITALL) != sizeof(int))
				break;
			
			data = ntohl(data);
			if(data == client->ch)
				printlg(LOG_DATA, "Stream heartbeat from client %s.", uuidstring(client->uuid));
			else if(data >= 0 && data < chcount)
			{
				remnode(client->ch, client);
				client->ch = data;
				addnode(client->ch, client);
			}
			else
				printlg(LOG_WARNING, "Invalid channel info from client %s.", uuidstring(client->uuid));
		}
		else
			printlg(LOG_WARNING, "Broken data from client %s.", uuidstring(client->uuid));
	}
	
	printlg(LOG_INFO, "Client %s disconnected.", uuidstring(client->uuid));
	remnode(client->ch, client);
	close(client->stream);
	free(client->dgram);
	free(client);
	return NULL;
}

void* metaconn(void *param)
{
	printlg(LOG_NOTICE, "Started TCP thread.");
	
	struct sockaddr_in saddr, caddr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	memset(&saddr, 0, addrlen);
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(1238);
	
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	fcntl(sfd, F_SETFD, FD_CLOEXEC);
	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	bind(sfd, (struct sockaddr*)&saddr, addrlen);
	listen(sfd, SOMAXCONN);
	
	printlg(LOG_DEBUG, "Initialized TCP server.");
	
	while(1) //TODO: Move everything to cliops
	{
		PNode client = malloc(sizeof(Node));
		client->ch = 0;
		client->next = NULL;
		client->dgram = NULL;
		uuid_generate(client->uuid);
		printlg(LOG_DEBUG, "Waiting for connection...");
		
		client->stream = accept(sfd, (struct sockaddr*)&caddr, &addrlen);
		fcntl(client->stream, F_SETFD, FD_CLOEXEC);
		client->lastup = time(NULL);
		client->port = ntohs(caddr.sin_port);
		inet_ntop(AF_INET, &caddr.sin_addr, client->ip, INET_ADDRSTRLEN);
		
		if(send(client->stream, client->uuid, sizeof(uuid_t), 0) == sizeof(uuid_t)) //Send UUID
			pthread_create(&client->thread, NULL, cliops, (void*)client);
		else
		{
			printlg(LOG_INFO, "Client %s failed to connect.", uuidstring(client->uuid));
			close(client->stream);
			free(client->dgram);
			free(client);
		}
	}
}

void* dataconn(void *param)
{
	printlg(LOG_NOTICE, "Started UDP thread.");
	
	struct sockaddr_in saddr, caddr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	memset(&saddr, 0, addrlen);
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	
	saddr.sin_port = htons(1873);
	int sfd = socket(AF_INET, SOCK_DGRAM, 0);
	fcntl(sfd, F_SETFD, FD_CLOEXEC);
	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	bind(sfd, (struct sockaddr*)&saddr, addrlen);
	
	int op, ch;
	ssize_t size, frame, limit = 32768;
	char data[limit];
	PNode itr, node;
	uuid_t uuid;
	printlg(LOG_DEBUG, "Initialized UDP server.");
	
	while(1) //TODO: Needs complete rework
	{
		size = recvfrom(sfd, data, limit, 0, (struct sockaddr*)&caddr, &addrlen);
		op = ntohl(*(int*)data);
		ch = ntohl(*(int*)(data + sizeof(int)));
		memcpy(&uuid, data + sizeof(int) * 2, sizeof(uuid_t));
		printlg(LOG_DEBUG, "Read %d bytes: op=%c, ch=%d, uuid=%s.", size, op, ch, uuidstring(uuid));
		
		pthread_rwlock_rdlock(&listlock[ch]); //Causes 2 read locks
		node = findnode(ch, uuid);
		if(node && op == 'u') //Update datagram entry
		{
			if(node->dgram)
				printlg(LOG_DATA, "Datagram heartbeat from client %s.", uuidstring(node->uuid));
			else
			{
				node->dgram = calloc(1, addrlen);
				memcpy(node->dgram, &caddr, addrlen);
				printlg(LOG_INFO, "Initialized datagram entry of client %s.", uuidstring(node->uuid));
			}
		}
		else if(node && op == 'b') //Broadcast to channel
		{
			itr = channel[ch]->next;
			frame = sizeof(int) * 2 + sizeof(uuid_t);
			printlg(LOG_DATA, "Broadcasting to channel %d from %s.", ch, uuidstring(node->uuid));
			for(int i=0; i<size-frame; i++)
				printf("%d ", *(data+frame+i));
			printf("\n");
			while(itr)
			{
				if(itr->dgram) //&& itr != node) //Disabled for test with only one client
					sendto(sfd, data + frame, size - frame, 0, (struct sockaddr*)itr->dgram, addrlen);
				itr = itr->next;
			}
		}
		else
			printlg(LOG_WARNING, "Broken packet from client %s.", uuidstring(uuid));
		pthread_rwlock_unlock(&listlock[ch]);
	}
}

int main(int argc, char *argv[]) //TODO: Need a garbage collector thread
{
	loglvl = LOG_DEF;
	printlg(LOG_NOTICE, "Started main thread with %ld cores online.", sysconf(_SC_NPROCESSORS_ONLN));
	
	chcount = 10000;
	channel = malloc(chcount * sizeof(PNode));
	listlock = malloc(chcount * sizeof(pthread_rwlock_t));
	//sem_init(&dataqueue, 0, 0);
	
	for(int i=0; i<chcount; i++)
	{
		channel[i] = calloc(1, sizeof(Node));
		pthread_rwlock_init(&listlock[i], NULL);
	}
	
	printlg(LOG_DEBUG, "Allocated channel lists.");
	
	pthread_create(&metathrd, NULL, metaconn, NULL);
	pthread_create(&datathrd, NULL, dataconn, NULL);
	
	while(1)
	{
		printlg(LOG_DEBUG, "Waiting for command...");
		
		char cmd;
		scanf(" %c", &cmd);
		
		if(cmd == 't') //Time
			printlg(LOG_INFO, "Timestamp: %s", timestamp());
		else if(cmd == 'l') //Log
		{
			scanf(" %d", &loglvl);
			if(loglvl < LOG_EMERG || loglvl > LOG_DATA)
			{
				printlg(LOG_WARNING, "Invalid log level!");
				loglvl = LOG_DEF;
			}
			printlg(LOG_INFO, "Log level changed to %d.", loglvl);
		}
		else if(cmd == 'd')
		{
			int ch;
			scanf(" %d", &ch);
			dumpch(ch);
		}
		else if(cmd == 'c') //Compile
		{
			printlg(LOG_NOTICE, "Compiling...");
			if(fork() == 0) //Optional: -std=gnu17 -march=native -Ofast -Wextra
				execl("/bin/gcc", "gcc", "server.c", "-o", "cast.o", "-luuid", "-lpthread", "-Wall", NULL);
			waitpid(-1, &(int){0}, 0);
		}
		else if(cmd == 'r') //Restart
		{
			printlg(LOG_NOTICE, "Restarting...");
			execl("./cast.o", "cast.o", NULL);
		}
		else if(cmd == 'e' || cmd == 'q') //Quit
		{
			printlg(LOG_NOTICE, "Shutting down!");
			break;
		}
		else //if(cmd != 't' && cmd != 'l' && cmd != 'c' && cmd != 'r' && cmd != 'e' && cmd != 'q')
			printlg(LOG_WARNING, "Invalid command!");
	}
	
	return 0;
}
