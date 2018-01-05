#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#include "server.h"
#include "connect_clients.h"
#include "tic_tac_toe.h"

void InitServer(char *argv[]);
void InitServerForNewGame();
void HandleServer();
void CreateSocketBindAndListen();
void SetSockAddrInAndBind();
void SetSocketToListen();
void InitBoard();
void CloseSocketsAndThreads();

void InitServer(char *argv[]) {
	Server.ListeningSocket = INVALID_SOCKET;
	int ClientIndex = 0;
	for (; ClientIndex < NUMBER_OF_CLIENTS; ClientIndex++) {
		Server.ClientsSockets[ClientIndex] = INVALID_SOCKET;
		Server.ClientsThreadHandle[ClientIndex] = NULL;
		Server.Players[ClientIndex].ClientIndex = ClientIndex;
	}
	Server.ConnectUsersThreadHandle = NULL;
	Server.LogFilePtr = argv[1];
	Server.PortNum = atoi(argv[2]);
	Server.WaitingForClientsTimedOut = false;

	Server.NumberOfConnectedUsersSemaphore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - not signaled */
		1,		/* Maximum Count */
		NULL);	/* un-named */
	if (Server.NumberOfConnectedUsersSemaphore == NULL) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error when creating NumberOfConnectedUsers semaphore.\n");
		exit(ERROR_CODE);
	}
	Server.ServerPropertiesUpdatesMutex = CreateMutex(
		NULL,	/* default security attributes */
		FALSE,	/* initially not owned */
		NULL);	/* unnamed mutex */
	if (NULL == Server.ServerPropertiesUpdatesMutex) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error when creating ServerPropertiesUpdates mutex.\n");
		exit(ERROR_CODE);
	}
	InitLogFile(Server.LogFilePtr);
}

void InitServerForNewGame() {
	InitBoard(); // init for new game
	Server.NumberOfConnectedUsers = 0;
	Server.Turn = X;
	Server.GameStatus = NotStarted;
	int ClientIndex = 0;
	for (; ClientIndex < NUMBER_OF_CLIENTS; ClientIndex++) {
		Server.Players[ClientIndex].PlayerType = None;
	}
}

void HandleServer() {
	DWORD wait_code;
	
	int GameIndex = 0;
	for (; GameIndex < NUMBER_OF_GAMES; GameIndex++) {
		CreateSocketBindAndListen();
		InitServerForNewGame();
		HandleConnectToClients();

		wait_code = WaitForMultipleObjects(NUMBER_OF_CLIENTS, Server.ClientsThreadHandle, TRUE, INFINITE);
		if (WAIT_OBJECT_0 != wait_code) {
			WriteToLogFile(Server.LogFilePtr, "Custom message: Error when waiting for program to end.\n");
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
	}
}

void CreateSocketBindAndListen() {
	InitWsaData();
	Server.ListeningSocket = CreateOneSocket();
	if (Server.ListeningSocket == INVALID_SOCKET) {
		char ErrorMessage[MESSAGE_LENGTH];
		sprintf(ErrorMessage, "Custom message: CreateSocketBindAndListen failed to create socket. Error Number is %d\n", WSAGetLastError());
		WriteToLogFile(Server.LogFilePtr, ErrorMessage);
		exit(ERROR_CODE);
	}

	SetSockAddrInAndBind();
	SetSocketToListen();
}

void SetSockAddrInAndBind() {
	int BindingReturnValue;

	Server.ListeningSocketService.sin_family = AF_INET;
	Server.ListeningSocketService.sin_addr.s_addr = inet_addr(SERVER_ADDRESS_STR); // todo check
	Server.ListeningSocketService.sin_port = htons(Server.PortNum);
	BindingReturnValue = bind(Server.ListeningSocket, (SOCKADDR*)&Server.ListeningSocketService,
							  sizeof(Server.ListeningSocketService));
	if (BindingReturnValue != BINDING_SUCCEEDED) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
}

void SetSocketToListen() {
	int ListenReturnValue;
	ListenReturnValue = listen(Server.ListeningSocket, NUMBER_OF_CLIENTS);
	if (ListenReturnValue != LISTEN_SUCCEEDED) {
		char ErrorMessage[MESSAGE_LENGTH];
		sprintf(ErrorMessage, "Custom message: SetSocketToListen failed to set Socket to listen. Error Number is %d\n", WSAGetLastError());
		WriteToLogFile(Server.LogFilePtr, ErrorMessage);
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
}

void InitBoard() {
	int RowNum = 0;
	for (; RowNum < BOARD_SIZE; RowNum++) {
		for (int ColumnNum = 0; ColumnNum < BOARD_SIZE; ColumnNum++) {
			Server.Board[RowNum][ColumnNum] = None;
		}
	}
}

void CloseSocketsAndThreads() {
	int ClientIndex = 0;
	for (; ClientIndex < NUMBER_OF_CLIENTS; ClientIndex++) {
		CloseOneSocket(Server.ClientsSockets[ClientIndex], Server.LogFilePtr);
		CloseOneThreadHandle(Server.ClientsThreadHandle[ClientIndex], Server.LogFilePtr);
	}
	CloseOneThreadHandle(Server.ServerPropertiesUpdatesMutex, Server.LogFilePtr);
	CloseOneSocket(Server.ListeningSocket, Server.LogFilePtr);
	
	CloseWsaData(Server.LogFilePtr);
}