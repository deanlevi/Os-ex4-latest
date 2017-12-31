#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h> // todo remove if removing printf
#include <stdlib.h>
#include <stdbool.h>

#include "server.h"

void InitServer(char *argv[]);
void HandleServer();
void CreateSocketBindAndListen();
void SetSockAddrInAndBind();
void SetSocketToListen();
void HandleConnectToClients();
void WINAPI ConnectToClientsThread(LPVOID lpParam);
void InitBoard();
void WINAPI TicTacToeGameThread(LPVOID lpParam);
bool HandleNewUserRequestAndAccept(int ClientIndex);
void ParseNewUserRequest(char *ReceivedData, int ClientIndex);
void UpdateNumberOfConnectedUsersAndWaitForGameStart();
void WaitForGameStartedSignal();
void SignalGameStarted();
void SendGameStartedBoardViewAndTurnSwitch(int ClientIndex);
void SendBoardView(int ClientIndex);
void SendTurnSwitch(int ClientIndex);
void HandleReceivedData(char *ReceivedData, int ClientIndex);
void HandlePlayRequest(char *ReceivedData);
void HandleUserListQuery();
void CloseSocketsAndThreads();

void InitServer(char *argv[]) {
	Server.ListeningSocket = INVALID_SOCKET;
	int ClientIndex = 0;
	for (; ClientIndex < NUMBER_OF_CLIENTS; ClientIndex++) {
		Server.ClientsSockets[ClientIndex] = INVALID_SOCKET;
		Server.ClientsThreadHandle[ClientIndex] = NULL;
		Server.Players[ClientIndex].ClientIndex = ClientIndex;
		Server.Players[ClientIndex].PlayerType = None;
	}
	Server.ConnectUsersThreadHandle = NULL;
	Server.LogFilePtr = argv[1];
	Server.PortNum = atoi(argv[2]);
	Server.Turn = 0;
	Server.GameStartedSemaphore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - not signaled */
		1,		/* Maximum Count */
		NULL);	/* un-named */
	if (Server.GameStartedSemaphore == NULL) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error when creating GameStarted semaphore.\n");
		exit(ERROR_CODE);
	}
	Server.NumberOfConnectedUsersSemaphore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - not signaled */
		1,		/* Maximum Count */
		NULL);	/* un-named */
	if (Server.GameStartedSemaphore == NULL) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error when creating NumberOfConnectedUsers semaphore.\n");
		exit(ERROR_CODE);
	}
	Server.NumberOfConnectedUsers = 0;
	Server.NumberOfConnectedUsersMutex = CreateMutex(
		NULL,	/* default security attributes */
		FALSE,	/* initially not owned */
		NULL);	/* unnamed mutex */
	if (NULL == Server.NumberOfConnectedUsersMutex) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error when creating NumberOfConnectedUsers mutex.\n");
		exit(ERROR_CODE);
	}
	InitLogFile(Server.LogFilePtr);
}

void HandleServer() {
	DWORD wait_code;

	CreateSocketBindAndListen();

	int GameIndex = 0;
	for (; GameIndex < NUMBER_OF_GAMES; GameIndex++) {
		InitBoard(); // init for new game
		HandleConnectToClients();

		wait_code = WaitForMultipleObjects(NUMBER_OF_CLIENTS, Server.ClientsThreadHandle, TRUE, INFINITE); // todo check INFINITE
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
		CloseSocketsAndThreads(); // todo add print
		exit(ERROR_CODE);
	}
}

void SetSocketToListen() {
	int ListenReturnValue;
	ListenReturnValue = listen(Server.ListeningSocket, NUMBER_OF_CLIENTS); // todo check NUMBER_OF_CLIENTS
	if (ListenReturnValue != LISTEN_SUCCEEDED) {
		char ErrorMessage[MESSAGE_LENGTH];
		sprintf(ErrorMessage, "Custom message: SetSocketToListen failed to set Socket to listen. Error Number is %d\n", WSAGetLastError());
		WriteToLogFile(Server.LogFilePtr, ErrorMessage);
		CloseSocketsAndThreads(); // todo add/check print
		exit(ERROR_CODE);
	}
}

void HandleConnectToClients() {
	DWORD wait_code;
	int ClientIndex = 0;
	DWORD TimeToWait;
	for (; ClientIndex < NUMBER_OF_CLIENTS; ClientIndex++) { // connect each client with different waiting time
		Server.ConnectUsersThreadHandle = CreateThreadSimple((LPTHREAD_START_ROUTINE)ConnectToClientsThread,
														     &Server.Players[ClientIndex].ClientIndex,
														     &Server.ConnectUsersThreadID,
															  Server.LogFilePtr);
		if (Server.ConnectUsersThreadHandle == NULL) {
			CloseSocketsAndThreads(); // todo check if add function to handle error
			exit(ERROR_CODE);
		}
		TimeToWait = ClientIndex == 0 ? MINUTE_IN_MS : (4 * MINUTE_IN_MS); // wait one minute for first client and four more for second
		wait_code = WaitForSingleObject(Server.ConnectUsersThreadHandle, TimeToWait); // wait one minute for first client
		if (WAIT_OBJECT_0 != wait_code) {
			OutputMessageToWindowAndLogFile(Server.LogFilePtr, "No players connected. Exiting.\n");
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		CloseOneThreadHandle(Server.ConnectUsersThreadHandle, Server.LogFilePtr); // close thread handle
	}
}

void WINAPI ConnectToClientsThread(LPVOID lpParam) { // todo add support for more than one game / iteration
	int *ClientIndex = (int*)lpParam;
	while (Server.NumberOfConnectedUsers <= *ClientIndex) { // while clients are declined
		Server.ClientsSockets[*ClientIndex] = accept(Server.ListeningSocket, NULL, NULL); // todo check NULL, NULL
		if (Server.ClientsSockets[*ClientIndex] == INVALID_SOCKET) {
			char ErrorMessage[MESSAGE_LENGTH];
			sprintf(ErrorMessage, "Custom message: ConnectToClients failed to accept. Error Number is %d\n", WSAGetLastError());
			WriteToLogFile(Server.LogFilePtr, ErrorMessage);
			CloseSocketsAndThreads(); // todo add/check print
			exit(ERROR_CODE);
		}
		Server.ClientsThreadHandle[*ClientIndex] = CreateThreadSimple((LPTHREAD_START_ROUTINE)TicTacToeGameThread,
																	  &Server.Players[*ClientIndex].ClientIndex,
																   	  &Server.ClientsThreadID[*ClientIndex],
																	   Server.LogFilePtr);
		if (Server.ClientsThreadHandle[*ClientIndex] == NULL) {
			CloseSocketsAndThreads(); // todo check if add function to handle error
			exit(ERROR_CODE);
		}
		DWORD wait_code;
		wait_code = WaitForSingleObject(Server.NumberOfConnectedUsersSemaphore, INFINITE); // wait for signal
		if (WAIT_OBJECT_0 != wait_code) {
			WriteToLogFile(Server.LogFilePtr, "Custom message: Error when waiting for NumberOfConnectedUsers semaphore.\n");
			CloseSocketsAndThreads(); // todo check how to handle error by instructions
			exit(ERROR_CODE);
		}
	}
}

void InitBoard() {
	int ColumnNum = 0;
	int RowNum = 0;
	for (; RowNum < BOARD_SIZE; RowNum++) {
		for (; ColumnNum < BOARD_SIZE; ColumnNum++) {
			Server.Board[RowNum][ColumnNum] = None;
		}
	}
}

void WINAPI TicTacToeGameThread(LPVOID lpParam) {
	if (NULL == lpParam) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error in TicTacToeGame. Received null pointer.\n");
		CloseSocketsAndThreads(); // todo add/check print
		exit(ERROR_CODE);
	}
	int *ClientIndex = (int*)lpParam;
	bool UserWasAccepted = HandleNewUserRequestAndAccept(*ClientIndex);
	if (!UserWasAccepted) {
		if (ReleaseOneSemaphore(Server.NumberOfConnectedUsersSemaphore) == FALSE) { // to signal to ConnectUsersThread
			WriteToLogFile(Server.LogFilePtr, "Custom message: failed to release NumberOfConnectedUsers semaphore.\n");
			CloseSocketsAndThreads(); // todo check if add function to handle error
			exit(ERROR_CODE);
		}
		return; // finish thread. see how to move creating thread loop to a thread that will be able to continue waiting for clients
	}
	UpdateNumberOfConnectedUsersAndWaitForGameStart();
	SendGameStartedBoardViewAndTurnSwitch(*ClientIndex);

	while (TRUE) { // todo while !Server.GameEnded?
		char *ReceivedData = ReceiveData(Server.ClientsSockets[*ClientIndex], Server.LogFilePtr);
		if (ReceivedData == NULL) {
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		HandleReceivedData(ReceivedData, *ClientIndex);
	}
}

bool HandleNewUserRequestAndAccept(int ClientIndex) {
	char *ReceivedData = ReceiveData(Server.ClientsSockets[ClientIndex], Server.LogFilePtr);
	if (ReceivedData == NULL) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	ParseNewUserRequest(ReceivedData, ClientIndex);
	bool UserNameIsTaken = (ClientIndex == 1) && (strcmp(Server.Players[0].UserName, Server.Players[1].UserName) == 0);
	if (UserNameIsTaken) { // todo maybe change to less duplicate code
		int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], "NEW_USER_DECLINED\n", Server.LogFilePtr);
		if (SendDataToServerReturnValue == ERROR_CODE) {
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		char TempMessage[MESSAGE_LENGTH];
		sprintf(TempMessage, "Custom message: Sent NEW_USER_DECLINED to Client %d, UserName %s.\n",
						      ClientIndex, Server.Players[ClientIndex].UserName);
		WriteToLogFile(Server.LogFilePtr, TempMessage);
		return false;
	}
	else {
		char NewUserAcceptedMessage[MESSAGE_LENGTH];
		Server.Players[ClientIndex].PlayerType = ClientIndex == 0 ? X : O;
		if (Server.Players[ClientIndex].PlayerType == X) {
			sprintf(NewUserAcceptedMessage, "NEW_USER_ACCEPTED:x;1\n");
		}
		else {
			sprintf(NewUserAcceptedMessage, "NEW_USER_ACCEPTED:o;2\n");
		}
		int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], NewUserAcceptedMessage, Server.LogFilePtr);
		if (SendDataToServerReturnValue == ERROR_CODE) {
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		char TempMessage[MESSAGE_LENGTH];
		sprintf(TempMessage, "Custom message: Sent NEW_USER_ACCEPTED to Client %d, UserName %s.\n",
							  ClientIndex, Server.Players[ClientIndex].UserName);
		WriteToLogFile(Server.LogFilePtr, TempMessage);
		return true;
	}
	return true; // to remove warning
}

void ParseNewUserRequest(char *ReceivedData, int ClientIndex) {
	int StartPosition = 0;
	int EndPosition = 0;
	int ParameterSize;
	while (ReceivedData[EndPosition] != ':') { // assuming valid input
		EndPosition++;
	}
	ParameterSize = (EndPosition - 1) - StartPosition + 1;
	if (strncmp(ReceivedData, "NEW_USER_REQUEST", ParameterSize) != 0) {
		char ErrorMessage[MESSAGE_LENGTH];
		sprintf(ErrorMessage, "Custom message: Got unexpected data from client number %d. Exiting...\n", ClientIndex);
		WriteToLogFile(Server.LogFilePtr, ErrorMessage);
		free(ReceivedData);
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	EndPosition++;
	StartPosition = EndPosition;
	while (ReceivedData[EndPosition] != '\n') { // assuming valid input
		EndPosition++;
	}
	ParameterSize = (EndPosition - 1) - StartPosition + 1;
	strncpy(Server.Players[ClientIndex].UserName, ReceivedData + StartPosition, ParameterSize);
	Server.Players[ClientIndex].UserName[EndPosition] = '\0';
	free(ReceivedData);
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Received NEW_USER_REQUEST from Client %d, UserName %s.\n",
						  ClientIndex, Server.Players[ClientIndex].UserName);
	WriteToLogFile(Server.LogFilePtr, TempMessage);
}

void UpdateNumberOfConnectedUsersAndWaitForGameStart() {
	DWORD wait_code;
	BOOL ret_val;

	wait_code = WaitForSingleObject(Server.NumberOfConnectedUsersMutex, INFINITE); // wait for Mutex access
	if (WAIT_OBJECT_0 != wait_code) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error in wait for NumberOfConnectedUsersMutex.\n");
		CloseSocketsAndThreads(); // todo add/check print
		exit(ERROR_CODE);
	}
	Server.NumberOfConnectedUsers++;
	if (ReleaseOneSemaphore(Server.NumberOfConnectedUsersSemaphore) == FALSE) { // to signal to ConnectUsersThread
		WriteToLogFile(Server.LogFilePtr, "Custom message: failed to release NumberOfConnectedUsers semaphore.\n");
		CloseSocketsAndThreads(); // todo check if add function to handle error
		exit(ERROR_CODE);
	}
	if (Server.NumberOfConnectedUsers == 2) { // can start game
		SignalGameStarted();
		ret_val = ReleaseMutex(Server.NumberOfConnectedUsersMutex); // release mutex
		if (FALSE == ret_val) {
			WriteToLogFile(Server.LogFilePtr, "Custom message: Error in releasing NumberOfConnectedUsersMutex.\n");
			CloseSocketsAndThreads(); // todo add/check print
			exit(ERROR_CODE);
		}
	}
	else { // wait for one more user to connect
		ret_val = ReleaseMutex(Server.NumberOfConnectedUsersMutex); // release mutex
		if (FALSE == ret_val) {
			WriteToLogFile(Server.LogFilePtr, "Custom message: Error in releasing NumberOfConnectedUsersMutex.\n");
			CloseSocketsAndThreads(); // todo add/check print
			exit(ERROR_CODE);
		}
		WaitForGameStartedSignal();
	}
}

void WaitForGameStartedSignal() {
	DWORD wait_code;
	wait_code = WaitForSingleObject(Server.GameStartedSemaphore, INFINITE); // wait for signal // todo check infinite - should change to 5 min?
	if (WAIT_OBJECT_0 != wait_code) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error when waiting for GameStarted semaphore.\n");
		CloseSocketsAndThreads(); // todo check how to handle error by instructions
		exit(ERROR_CODE);
	}
}

void SignalGameStarted() {
	if (ReleaseOneSemaphore(Server.GameStartedSemaphore) == FALSE) { // to signal that both clients connected
		WriteToLogFile(Server.LogFilePtr, "Custom message: SignalGameStarted - failed to release GameStarted semaphore.\n");
		CloseSocketsAndThreads(); // todo check if add function to handle error
		exit(ERROR_CODE);
	}
}

void SendGameStartedBoardViewAndTurnSwitch(int ClientIndex) {
	int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], "GAME_STARTED\n", Server.LogFilePtr);
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	SendBoardView(ClientIndex);
	SendTurnSwitch(ClientIndex);
}

void SendBoardView(int ClientIndex) {
	char BoardMessage[MESSAGE_LENGTH];
	char CharToUpdate;
	strcpy(BoardMessage, "BOARD_VIEW:| | | |");
	strcat(BoardMessage, "           | | | |");
	strcat(BoardMessage, "           | | | |\n");
	int ColumnNum = BOARD_SIZE - 1;
	int RowNum;
	for (; ColumnNum >= 0; ColumnNum--) {
		for (RowNum = 0; RowNum < BOARD_SIZE; RowNum++) {
			if (Server.Board[RowNum][ColumnNum] != None) {
				CharToUpdate = Server.Board[RowNum][ColumnNum] == X ? 'x' : 'o';
				BoardMessage[BOARD_MESSAGE_LINE_LENGTH*(BOARD_SIZE - 1 - ColumnNum) +
							 BOARD_MESSAGE_LINE_OFFSET + 2 * RowNum] = CharToUpdate;
			}
		}
	}

	int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], BoardMessage, Server.LogFilePtr);
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Sent BOARD_VIEW to Client %d, UserName %s.\n",
						  ClientIndex, Server.Players[ClientIndex].UserName);
	WriteToLogFile(Server.LogFilePtr, TempMessage);
}

void SendTurnSwitch(int ClientIndex) {
	char TurnMessage[MESSAGE_LENGTH];
	char PlayerType = Server.Players[ClientIndex].PlayerType == X ? 'x' : 'o';
	sprintf(TurnMessage, "TURN_SWITCH:%s;%c\n", Server.Players[ClientIndex].UserName, PlayerType);
	int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], TurnMessage, Server.LogFilePtr);
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Sent TURN_SWITCH to Client %d, UserName %s.\n",
						  ClientIndex, Server.Players[ClientIndex].UserName);
	WriteToLogFile(Server.LogFilePtr, TempMessage);
}

void HandleReceivedData(char *ReceivedData, int ClientIndex) {
	if (strstr(ReceivedData, "PLAY_REQUEST") == 0) {
		HandlePlayRequest(ReceivedData); // todo
	}
	else if (strstr(ReceivedData, "USER_LIST_QUERY") == 0) {
		HandleUserListQuery(); // todo
	}
	else if (strstr(ReceivedData, "GAME_STATE_QUERY") == 0) { // todo - not in the instruction list !!
		//HandleGameStateQuery(); // todo
	}
	else if (strstr(ReceivedData, "BOARD_VIEW_QUERY") == 0) { // todo - not in the instruction list !!
		//SendBoardView(ClientIndex); // todo
	}
	else {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Got unexpected answer from client. Exiting...\n");
		free(ReceivedData);
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	/*char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Received from server: %s\n", ReceivedData);
	WriteToLogFile(Server.LogFilePtr, TempMessage);*/
	free(ReceivedData);
}

void HandlePlayRequest(char *ReceivedData) {

}

void HandleUserListQuery() {

}

void CloseSocketsAndThreads() {
	int ClientIndex = 0;
	for (; ClientIndex < NUMBER_OF_CLIENTS; ClientIndex++) {
		CloseOneSocket(Server.ClientsSockets[ClientIndex], Server.LogFilePtr);
		CloseOneThreadHandle(Server.ClientsThreadHandle[ClientIndex], Server.LogFilePtr);
	}
	CloseOneThreadHandle(Server.GameStartedSemaphore, Server.LogFilePtr);
	CloseOneThreadHandle(Server.NumberOfConnectedUsersMutex, Server.LogFilePtr);
	CloseOneSocket(Server.ListeningSocket, Server.LogFilePtr);
	
	CloseWsaData(Server.LogFilePtr);
}