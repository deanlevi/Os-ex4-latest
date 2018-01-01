#ifndef SERVER_H
#define SERVER_H

#include "../Shared/socket.h" // todo

#define SERVER_ADDRESS_STR "127.0.0.1" // todo move to shared
#define NUMBER_OF_CLIENTS 2
#define NUMBER_OF_GAMES 1 // todo
#define MINUTE_IN_MS 60000
#define BINDING_SUCCEEDED 0
#define LISTEN_SUCCEEDED 0

typedef struct _PlayerProperties {
	char UserName[USER_NAME_LENGTH];
	PlayerType PlayerType; // X - first player, O - second player
	int ClientIndex; // to send to thread function
}PlayerProperties;

/*typedef struct _BoardProperties { // todo check if remove
	char UserName[USER_NAME_LENGTH];
	PlayerType PlayerType; // X - first player, O - second player
	int ClientIndex; // to send to thread function
}BoardProperties;*/

typedef struct _ServerProperties {
	////// sockets
	SOCKET ListeningSocket; // the socket that is listening to client connections
	SOCKADDR_IN ListeningSocketService;
	SOCKET ClientsSockets[NUMBER_OF_CLIENTS]; // the sockets that are connected to each client

	////// threads
	HANDLE ConnectUsersThreadHandle; // thread handle for connecting two users
	DWORD ConnectUsersThreadID; // thread id to the above thread handle
	HANDLE ClientsThreadHandle[NUMBER_OF_CLIENTS]; // thread handle to each client
	DWORD ClientsThreadID[NUMBER_OF_CLIENTS]; // thread id to each client

	////// semaphores and mutexes
	HANDLE NumberOfConnectedUsersSemaphore; // semaphore to signal ConnectUsersThread that NumberOfConnectedUsers was updated.
	HANDLE NumberOfConnectedUsersMutex; // mutex to write to NumberOfConnectedUsers
	
	////// others
	char *LogFilePtr; // path to log file
	int PortNum; // server's port num
	PlayerProperties Players[NUMBER_OF_CLIENTS];
	int NumberOfConnectedUsers;
	PlayerType Turn; // 0 - first player turn, 1 - second player turn
	PlayerType Board[BOARD_SIZE][BOARD_SIZE];
}ServerProperties;

ServerProperties Server;

void InitServer(char *argv[]);
void HandleServer();
void CloseSocketsAndThreads();

#endif