#include "vtmpi.h"
#include "../vtl_logic.h"
#include "../socket_layer/socket.h"
#include <poll.h>
#include <unistd.h>
#include <sys/syscall.h>

int serverSocket;
int rank;
int total;
char *ipAddress;
struct sockaddr_in serverAddr;
pthread_t *serverThread;
int *foundCondition;
int *sockets;
int bcast_tag = 1234;

struct peerNode *nodes;
struct message *messageList = NULL;
struct message *requestList = NULL;

//Add the receive request to the list in case the MPI_Recv is called before the actual message comes.
void addRequestToList(struct message *request) {
	if(requestList == NULL)
		requestList = request;
	else {
		struct message *temp = requestList;
		while(temp->next != NULL) {
			temp = temp->next;
		}
		temp->next = request;
	}
}

//Retrieve the request characterized by the given parameters.
struct message *getRequestFromList(int source, int size, int tag, int type) {
	if(requestList == NULL)
		return NULL;
	struct message *temp = requestList;
	if(	temp->source == source &&
		temp->size == size && 
		temp->tag == tag &&
		temp->type == type) {
		requestList = requestList->next;
		return temp;
	}
	while(temp->next != NULL) {
		if(temp->source == source && temp->next->size == size &&
		   temp->next->tag == tag && temp->next->type == type) {
			break;
		}
		temp = temp->next;
	}
	struct message *returnMessage = temp->next;
	if(temp->next != NULL)
		temp->next = temp->next->next;
	return returnMessage;
}

//Add the message to the list in case the MPI_Recv is called after the actual message comes.
void addMessageToList(struct message *recvdMessage) {
	if(messageList == NULL)
		messageList = recvdMessage;
	else {
		struct message *temp = messageList;
		while(temp->next != NULL) {
			temp = temp->next;
		}
		temp->next = recvdMessage;
	}
}

//Retrieve the message characterized by the given parameters.
struct message *getMessageFromList(int source, int size, int type, int tag) {
	if(messageList == NULL)
		return NULL;
	struct message *temp = messageList;
	if(temp->size == size && temp->tag == tag && 
	   temp->type == type && temp->source == source) {
		messageList = messageList->next;
		return temp;
	}
	while(temp->next != NULL) {
		if(temp->next->size == size &&
		   temp->next->tag == tag &&
		   temp->next->type == type &&
		   temp->next->source == source) {
			break;
		}
		temp = temp->next;
	}
	struct message *returnMessage = temp->next;
	if(temp->next != NULL)
		temp->next = temp->next->next;
	return returnMessage;
}

void getMessages(int listenRank) {
	int buffer[3] = {0};
	recv(sockets[listenRank], buffer, sizeof(int) * 3, 0);
	if(buffer[2] == 0) {
		printf ("Closing socket: %d\n", listenRank);
		fflush(stdout);
		close(sockets[listenRank]);
		sockets[listenRank] = -1;
		return;
	}
	struct message *request = getRequestFromList(listenRank, buffer[0], buffer[1], buffer[2]);
	if(request == NULL) {
		//Message comes before MPI_Recv called.
		struct message *recvdMessage = (struct message *)malloc(
			sizeof(struct message));
		recvdMessage->size = buffer[0];
		recvdMessage->tag = buffer[1];
		recvdMessage->type = buffer[2];
		recvdMessage->source = listenRank;
		recvdMessage->next = NULL;
		recvdMessage->data = NULL;
		addMessageToList(recvdMessage);
		char * data = malloc(recvdMessage->size * recvdMessage->type);
		recv(sockets[listenRank], data,
			recvdMessage->size * recvdMessage->type, 0);
		recvdMessage->data = data;
	}
	else {
		//MPI_Recv called before message comes.
		char * data = malloc(request->size * request->type);
		recv(sockets[listenRank], data,
			request->size * request->type, 0);
		request->data = data;
	}
	
	foundCondition[listenRank] = 1;
}

//Thread function to listen for messages from each peer.
void *listenForMessages(void* arg) {
	int ignoreRank = *((int *)arg);
	int numActiveSockets = total - 1;
	struct pollfd *pfds;
	int * mapping;
	pfds = calloc(numActiveSockets, sizeof(struct pollfd));
	mapping = calloc(numActiveSockets, sizeof(int));
	if (pfds == NULL)
		perror("malloc");
		
    int ThreadID = syscall(SYS_gettid);
        
	/* Open each file on command line, and add it 'pfds' array */
	do {
		int nfds = 0;
		for (int j = 0; j < total; j++) {
			if (j == ignoreRank || sockets[j] == -1)
				continue;

			pfds[nfds].fd = sockets[j];
			pfds[nfds].events = POLLIN;
			pfds[nfds].revents = 0;
			mapping[nfds] = j;

			nfds++;

		}
		int ready;
		#ifndef DISABLE_LOOKAHEAD
		SetLookahead(0, LOOKAHEAD_ANCHOR_EAT);
		#endif
		// Directly call _poll provided by vt-socket layer here.
		TriggerSyscallWait(ThreadID, 0);
		ready = _poll(pfds, nfds, 0);
		
		if (ready) {
			for (int i = 0; i < nfds; i++) {
				if (pfds[i].revents & POLLIN) {
					getMessages(mapping[i]);
				}
			}
		}
		numActiveSockets = 0;
		for (int i = 0; i < total; i++) {
			if (i == ignoreRank)
				continue;
			if (sockets[i] != -1)
				numActiveSockets ++;
		}
	} while(numActiveSockets > 0);

	printf ("All active listen sockets closed !\n");
	TriggerSyscallFinish(ThreadID);
	
	free(mapping);
	free(pfds);
	return NULL;
}

int MPI_Bcast(void *buffer, int count, int datatype, int root, int MPI_comm) {
	int start = 0;
	if (root == rank) {
		// sender
		while ( start < count ) {
			if (count - start < 100) {
				for (int i = 0; i < total; i++) {
					if (i == rank) continue;
					MPI_Send(buffer + datatype * start, count - start, datatype,
						i, bcast_tag, MPI_comm);

				}
				break;
			} else {
				for (int i = 0; i < total; i++) {
					if (i == rank) continue;
					MPI_Send(buffer + datatype * start, 100, datatype,
						i, bcast_tag, MPI_comm);

				}
				start += 100;
				bcast_tag ++;
			}
		}

	} else {
		while ( start < count ) {
			if (count - start < 100) {
				MPI_Recv(buffer + datatype * start, count - start, datatype, root,
					bcast_tag, MPI_comm, NULL);
				break; 
			} else {
				MPI_Recv(buffer + datatype * start, 100, datatype, root,
					bcast_tag, MPI_comm, NULL);
				start += 100;
				bcast_tag ++; 

			}
		}

	}
	return 0;
}

int MPI_Send(void *buf, int count, int datatype,
			int dest, int tag, int MPI_comm) {
	int buffer[3];
	buffer[0] = count;
	buffer[1] = tag;
	buffer[2] = datatype;
	send(sockets[dest], buffer, sizeof(int) * 3, 0);
	send(sockets[dest], buf, datatype * count, 0);
	return 0;
}

int MPI_Recv(void *buf, int count, int datatype,
	int source, int tag, int MPI_comm, void *status) {
	struct message *foundMessage;
	struct message request;
	int ThreadID = syscall(SYS_gettid);
	int toFree = 1;
	int blocked = 0;
	
	foundMessage = getMessageFromList(source, count, datatype, tag);
	//MPI_Recv called before message comes.
	if (!foundMessage) {
		request.data = NULL;
		request.size = count;
		request.tag = tag;
		request.type = datatype;
		request.source = source;
		request.next = NULL;
		addRequestToList(&request);
		foundMessage = &request;
		toFree = 0;
	}


	while (foundMessage->data == NULL) {
		#ifndef DISABLE_LOOKAHEAD
		SetLookahead(0, LOOKAHEAD_ANCHOR_EAT);
		#endif
		TriggerSyscallWait(ThreadID, 0);
		blocked = 1;
	}

	if (blocked)
		TriggerSyscallFinish(ThreadID);


	foundCondition[source] = 0;
	memcpy(buf, foundMessage->data, foundMessage->size * foundMessage->type);
	free(foundMessage->data);
	if (toFree)
		free(foundMessage);
	return 0;
}

void setupServerSocket() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	int port = 10000;
	do {
	    serverAddr.sin_family = AF_INET;
	    serverAddr.sin_port = htons(port);
	    serverAddr.sin_addr.s_addr = inet_addr(ipAddress);
	    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  
		port++;
	} while(bind(serverSocket, (struct sockaddr *) &serverAddr, 
		sizeof(serverAddr)) == -1);

	printf ("Server socket bound to ip:port: %s:%d\n", ipAddress, port - 1);
}

//Populate list of peers from the endpoints file.
void populatePeerList() {
	int i = 0, bufPos = 0, port = 0, rank = 0;
	FILE *fp = fopen("/tmp/endpoints", "r");
	char c;
	c = fgetc(fp);
	while(c != EOF) {
		while(c != ':') {
			rank = rank * 10 + (c - '0');
			c = fgetc(fp); 
		}
		nodes[i].rank = rank;
		c = fgetc(fp);
		while(c != ':') {
			nodes[i].IP[bufPos++] = c;
			c = fgetc(fp);
		}
		nodes[i].IP[bufPos] = '\0';
		c = fgetc(fp);
		while(c != '\n' && c != EOF) {
			port = port * 10 + (c - '0');
			c = fgetc(fp);
		}
		nodes[i].port = port;
		if(c == EOF) {
			fclose(fp);
			break;
		}
		else {
			port = 0;
			rank = 0;
			bufPos = 0;
			i++;
			c = fgetc(fp);
			continue;
		}
	}
}

//Last node to send to the endpoints file sends a message to all other nodes informing them that all nodes have entered the system.
void sendOk() {
	struct sockaddr_in serverAddr;
	int buffer;
	int i;
	for(i = 0; i<total; i++) {
		if(i == rank)
			continue;
		int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
		buffer = rank;
	    serverAddr.sin_family = AF_INET;
	    serverAddr.sin_port = htons(nodes[i].port);
	    serverAddr.sin_addr.s_addr = inet_addr(nodes[i].IP);
	    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
		printf ("Connecting to : %s:%d to send OK message\n", nodes[i].IP, nodes[i].port);
		connect(clientSocket, 
			(struct sockaddr *)&serverAddr, sizeof(serverAddr));
		send(clientSocket, &buffer, sizeof(int), 0);
		close(clientSocket);
		printf ("Successfully sent OK message !\n");
	}
}

//Nodes wait for all the nodes to enter the system.
void waitForOk(){
	int buffer;
	struct sockaddr_in clientAddr;
	socklen_t addr_size;
	addr_size = sizeof(clientAddr);
	printf ("Waiting for OK message !\n");
	listen(serverSocket, total);
	int confd = accept(serverSocket,
		(struct sockaddr *)&clientAddr, &addr_size);
	recv(confd, &buffer, sizeof(int), 0);
	close(confd);
	printf ("Successfully received OK message !\n");
}

//Add information about itself to the endpoints file.
void doFileWork() {
	int nodeNumber = 0;
	char buffer[20];
	FILE *fp = fopen("/tmp/endpoints", "r");
	if(fp != NULL) {
		char c;
		while((c = fgetc(fp)) != EOF) {
			if(c == '\n') {
				nodeNumber++;
			}
		}
		fclose(fp);
	
		fp = fopen("/tmp/endpoints", "a");
		fprintf(fp, "%d:%s:%d\n", rank, ipAddress, ntohs(serverAddr.sin_port));
		fclose(fp);
	}
	else {
		fp = fopen("/tmp/endpoints", "w");
		fprintf(fp, "%d:%s:%d\n", rank, ipAddress, ntohs(serverAddr.sin_port));
		fclose(fp);
	}

	if(nodeNumber + 1 == total) {
		populatePeerList();
		printf ("Last node: Sending Ok msg to all nodes!\n");
		sendOk();
	} else {
		printf ("Waiting for OK message from other nodes !\n");
		waitForOk();
		populatePeerList();
	}
}

//Connect to nodes ranked lower than itself.
void connectToLower() {  // useful for worker (rank = 1). sockets[0] is set
	struct sockaddr_in serverAddr;
	int buffer;
	int i;
	for(i = rank - 1; i>=0; i--) {
		int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
		buffer = rank;
	    	serverAddr.sin_family = AF_INET;
	    	serverAddr.sin_port = htons(nodes[i].port);
	    	serverAddr.sin_addr.s_addr = inet_addr(nodes[i].IP);
	    	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
		printf ("Connecting to lower ranked node: %d at %s:%d\n", i, nodes[i].IP, nodes[i].port);
		fflush(stdout);
		connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
		send(clientSocket, &buffer, sizeof(int), 0);
		printf ("Successfully connected to lower ranked node: %d at %s:%d\n", i, nodes[i].IP, nodes[i].port);
		fflush(stdout);
		sockets[i] = clientSocket;
	}
}

//Accept connections from nodes ranked higher than itself.
void acceptFromUpper() { // useful for master (rank = 0). sockets[1] is set
	int i;
	listen(serverSocket, total);
	for(i = total - 1; i>rank; i--) {
		int buffer;
		struct sockaddr_in clientAddr;
		socklen_t addr_size;
		addr_size = sizeof(clientAddr);
		
		printf ("Accepting connection from higher ranked node: %d\n", i);
		fflush(stdout);
		int confd = accept(serverSocket, 
			(struct sockaddr *)&clientAddr, &addr_size);
		printf ("Accepted connection from higher ranked node: %d\n", i);
		fflush(stdout);
		recv(confd, &buffer, sizeof(int), 0);
		sockets[buffer] = confd;
		printf ("Successfully connected with higher ranked node: %d\n", i);
		fflush(stdout);
	}	

	printf ("Closing server socket: %d\n", serverSocket);
	fflush(stdout);
	close(serverSocket);
}

int MPI_Comm_size(int MPI_Comm, int *size) {
	*size = total;
	return 0;
}

int MPI_Comm_rank(int MPI_Comm, int *worldRank) {
	*worldRank = rank;
	return 0;
}

int MPI_Get_processor_name(char *name, int *resultlen) {
	*resultlen = strlen(ipAddress);
	name = (char *)malloc(sizeof(char) * (*resultlen) + 1);
	strcpy(name, ipAddress);
	return 0;
}

int MPI_Init(int *argc, char **argv[]) {
	struct hostent *host;
	int i;
	rank = atoi((*argv)[1]);
	total = atoi((*argv)[2]);
	ipAddress = (*argv)[3];
	nodes = (struct peerNode *)malloc(sizeof(struct peerNode) * total);
	foundCondition = (int *)malloc(sizeof(int) * total);
	serverThread = (pthread_t *)malloc(sizeof(pthread_t));
	messageList = NULL;
	requestList = NULL;
	sockets = (int *)malloc(sizeof(int) * total);

	printf ("Rank = %d, Total = %d, IP = %s\n", rank, total, ipAddress);
	fflush(stdout);

	for(i = 0; i<total; i++) {
		sockets[i] = 0;
		foundCondition[i] = 0;
	}
	setupServerSocket();
	if (rank > 0)
		usleep(rank * 100);

	doFileWork();

	printf ("\n---------------- Waiting for higher ranked nodes to connect --------------\n");
	fflush(stdout);
	acceptFromUpper();
	//if(rank != total -1)
	printf ("\n---------------- Connecting to lowed ranked nodes ! --------------\n");
	fflush(stdout);
	connectToLower();

	int *ignoreRank = (int *)malloc(sizeof(int));
	*ignoreRank = rank;
	printf ("\n---------------- Creating listen Thread ! --------------\n");
	fflush(stdout);
	pthread_create(serverThread, NULL, listenForMessages, ignoreRank);

	printf ("\n---------------- MPI-Init success ! --------------\n");
	fflush(stdout);
	return 0;
}

double MPI_Wtime() {
	struct timeval tv;
	uint64_t timeInMicroseconds;
	double time;
    gettimeofday(&tv,NULL);
    timeInMicroseconds = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
    time = (double)timeInMicroseconds/(double)1000000;
    return time;
}

int MPI_Finalize() {
	int i;
	int buf[3] = {0};
	printf ("Calling MPI-Finalize !\n");
	fflush(stdout);
	for(i = 0; i< total; i++) {
		if(rank != i && sockets[i] > 0) {
			printf ("Sending close for socket: %d\n", sockets[i]);
			send(sockets[i], buf, sizeof(int) * 3, 0);
			close(sockets[i]);
		}
	}

	printf ("Waiting for listen-thread to exit !\n");
	fflush(stdout);
	pthread_join(*serverThread, NULL);

	printf ("MPI Exec environment terminated !\n");
	fflush(stdout);
	return 0;
}
