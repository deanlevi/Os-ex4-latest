#ifndef CLIENT_H
#define CLIENT_H

#include "../Shared/socket.h" // todo

#define NUMBER_OF_THREADS_TO_HANDLE_CLIENT 3
#define USER_INPUT_LENGTH 20 // todo check
#define GAME_STARTED_SIZE 12 // todo check if move to general defines
#define TURN_SWITCH_SIZE 12
#define BOARD_VIEW_SIZE 11
#define PLAY_ACCEPTED_SIZE 13
#define PLAY_DECLINED_SIZE 14
#define USER_LIST_REPLY_SIZE 16
#define GAME_STATE_REPLY_SIZE 17
#define NEW_USER_ACCEPTED_OR_DECLINED_SIZE 17
#define GAME_ENDED_SIZE 11

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