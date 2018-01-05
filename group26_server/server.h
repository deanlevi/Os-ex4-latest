#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>

#include "socket.h"

#define SERVER_ADDRESS_STR "127.0.0.1"
#define NUMBER_OF_CLIENTS 2
#define NUMBER_OF_GAMES 1 // todo
#define BINDING_SUCCEEDED 0
#define LISTEN_SUCCEEDED 0

typedef struct _PlayerProperties {
	char UserName[USER_NAME_LENGTH];
	PlayerType PlayerType; // X - first player, O - second player
	int ClientIndex; // to send to thread function
}PlayerProperties;

typedef enum _GameStatus {
	NotStarted,
	Started,
	Ended
}GameStatus;

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
	HANDLE ServerPropertiesUpdatesMutex; // mutex to update server properties
	
	////// others
	char *LogFilePtr; // path to log file
	int PortNum; // server's port num
	PlayerProperties Players[NUMBER_OF_CLIENTS];
	int NumberOfConnectedUsers;
	PlayerType Turn; // X - first player turn, O - second player turn
	PlayerType Board[BOARD_SIZE][BOARD_SIZE];
	GameStatus GameStatus;
	bool WaitingForClientsTimedOut;
}ServerProperties;

ServerProperties Server;

void InitServer(char *argv[]);
void HandleServer();
void CloseSocketsAndThreads();

#endif