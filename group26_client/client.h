#ifndef CLIENT_H
#define CLIENT_H

#include "../Shared/socket.h" // todo

#define NUMBER_OF_THREADS_TO_HANDLE_CLIENT 3 // one for send, one for receive, one for user interface

typedef struct ClientProperties {
	////// sockets
	SOCKET Socket; // the socket that is connecting to the server
	SOCKADDR_IN SocketService;

	////// threads
	HANDLE ThreadHandles[NUMBER_OF_THREADS_TO_HANDLE_CLIENT]; // one for send, one for receive, and one for user interface
	DWORD ThreadIDs[NUMBER_OF_THREADS_TO_HANDLE_CLIENT]; // thread ids for the above thread handles

	////// semaphores and mutexes
	HANDLE UserInterfaceSemaphore; // semaphore to block user interface thread until connection is established and user is accepted
	HANDLE SendToServerSemaphore; // semaphore to signal each time a new message to send to server is available
	//HANDLE SendReceiveMutex; // mutex to sync processes using the same program // todo check

	////// others
	char *LogFilePtr; // path to log file
	char *ServerIP; // server's IP
	int ServerPortNum; // server's port num
	char *UserName; // todo why need to assume <= 30 ?
	PlayerType PlayerType;
	char MessageToSendToServer[MESSAGE_LENGTH]; // each time SendToServerSemaphore is signaled contains the message to send
}ClientProperties;

ClientProperties Client;

void InitClient(char *argv[]);
void HandleClient();
void CloseSocketAndThreads();

#endif