#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h> // todo remove if removing printf
#include <stdlib.h>
#include <stdbool.h>

#include "server.h"

void InitServer(char *argv[]);
void InitServerForNewGame();
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
void UpdateNumberOfConnectedUsers();
void SendGameStartedBoardViewAndTurnSwitch(int ClientIndex);
void SendBoardView(int ClientIndex, bool IsBoardViewQuery);
void SendTurnSwitch(int ClientIndex);
void HandleReceivedData(char *ReceivedData, int ClientIndex);
void HandlePlayRequest(int ClientIndex, char *ReceivedData);
void CheckIfGameEnded();
void HandleGameEndedMessage(bool OWon, bool XWon, bool BoardIsFull);
void HandleUserListQuery(int ClientIndex);
void HandleGameStateQuery(int ClientIndex);
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

	Server.NumberOfConnectedUsersSemaphore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - not signaled */
		1,		/* Maximum Count */
		NULL);	/* un-named */
	if (Server.NumberOfConnectedUsersSemaphore == NULL) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error when creating NumberOfConnectedUsers semaphore.\n");
		exit(ERROR_CODE);
	}
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

	CreateSocketBindAndListen();

	int GameIndex = 0;
	for (; GameIndex < NUMBER_OF_GAMES; GameIndex++) {
		InitServerForNewGame();
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
	int *ClientIndexPtr = (int*)lpParam;
	bool UserWasAccepted = HandleNewUserRequestAndAccept(*ClientIndexPtr);
	if (!UserWasAccepted) {
		if (ReleaseOneSemaphore(Server.NumberOfConnectedUsersSemaphore) == FALSE) { // to signal to ConnectUsersThread
			WriteToLogFile(Server.LogFilePtr, "Custom message: failed to release NumberOfConnectedUsers semaphore.\n");
			CloseSocketsAndThreads(); // todo check if add function to handle error
			exit(ERROR_CODE);
		}
		return; // finish thread. see how to move creating thread loop to a thread that will be able to continue waiting for clients
	}
	UpdateNumberOfConnectedUsers();
	SendGameStartedBoardViewAndTurnSwitch(*ClientIndexPtr);

	while (TRUE) { // todo while !Server.GameEnded?
		char *ReceivedData = ReceiveData(Server.ClientsSockets[*ClientIndexPtr], Server.LogFilePtr);
		if (ReceivedData == NULL) {
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		if (strcmp(ReceivedData, "FINISHED") == 0) {
			break; // finished communication
		}
		HandleReceivedData(ReceivedData, *ClientIndexPtr);
		if (Server.GameStatus == Ended) {
			for (int Client = 0; Client < NUMBER_OF_CLIENTS; Client++) {
				shutdown(Server.ClientsSockets[Client], SD_SEND);
			}
		}
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

void UpdateNumberOfConnectedUsers() {
	DWORD wait_code;
	BOOL ret_val;

	wait_code = WaitForSingleObject(Server.NumberOfConnectedUsersMutex, INFINITE); // wait for Mutex access
	if (WAIT_OBJECT_0 != wait_code) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error in wait for NumberOfConnectedUsersMutex.\n");
		CloseSocketsAndThreads(); // todo add/check print
		exit(ERROR_CODE);
	}
	Server.NumberOfConnectedUsers++;
	if (Server.NumberOfConnectedUsers == 2) {
		Server.GameStatus = Started; // todo verify that here.
	}
	if (ReleaseOneSemaphore(Server.NumberOfConnectedUsersSemaphore) == FALSE) { // to signal to ConnectUsersThread
		WriteToLogFile(Server.LogFilePtr, "Custom message: failed to release NumberOfConnectedUsers semaphore.\n");
		CloseSocketsAndThreads(); // todo check if add function to handle error
		exit(ERROR_CODE);
	}
	ret_val = ReleaseMutex(Server.NumberOfConnectedUsersMutex); // release mutex
	if (FALSE == ret_val) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error in releasing NumberOfConnectedUsersMutex.\n");
		CloseSocketsAndThreads(); // todo add/check print
		exit(ERROR_CODE);
	}
}

void SendGameStartedBoardViewAndTurnSwitch(int ClientIndex) {
	int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], "GAME_STARTED\n", Server.LogFilePtr);
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	Sleep(START_MESSAGES_WAIT);
	SendBoardView(ClientIndex, false);
	Sleep(START_MESSAGES_WAIT);
	SendTurnSwitch(ClientIndex);
}

void SendBoardView(int ClientIndex, bool IsBoardViewQuery) {
	char BoardMessage[MESSAGE_LENGTH];
	int LineOffset, LineLength;
	char CharToUpdate;
	if (IsBoardViewQuery) {
		strcpy(BoardMessage, "BOARD_VIEW_REPLY:| | | |");
		strcat(BoardMessage, "                 | | | |");
		strcat(BoardMessage, "                 | | | |\n");
		LineOffset = BOARD_VIEW_LINE_OFFSET;
		LineLength = BOARD_VIEW_QUERY_LINE_LENGTH;
	}
	else {
		strcpy(BoardMessage, "BOARD_VIEW:| | | |");
		strcat(BoardMessage, "           | | | |");
		strcat(BoardMessage, "           | | | |\n");
		LineOffset = BOARD_VIEW_LINE_OFFSET;
		LineLength = BOARD_VIEW_LINE_LENGTH;
	}
	int ColumnNum = BOARD_SIZE - 1;
	int RowNum;
	for (; ColumnNum >= 0; ColumnNum--) {
		for (RowNum = 0; RowNum < BOARD_SIZE; RowNum++) {
			if (Server.Board[RowNum][ColumnNum] != None) {
				CharToUpdate = Server.Board[RowNum][ColumnNum] == X ? 'x' : 'o';
				BoardMessage[LineLength*(BOARD_SIZE - 1 - ColumnNum) + (LineOffset + 1) + 2 * RowNum] = CharToUpdate;
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
	char TypeOfPlayerWhoHasTurn = Server.Turn == X ? 'x' : 'o';
	char *UserNameOfPlayerWhoHasTurn = Server.Turn == X ? Server.Players[0].UserName : Server.Players[1].UserName;
	sprintf(TurnMessage, "TURN_SWITCH:%s;%c\n", UserNameOfPlayerWhoHasTurn, TypeOfPlayerWhoHasTurn);
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
	if (strncmp(ReceivedData, "PLAY_REQUEST:", PLAY_REQUEST_SIZE) == 0) {
		HandlePlayRequest(ClientIndex, ReceivedData); // todo
	}
	else if (strncmp(ReceivedData, "USER_LIST_QUERY", USER_LIST_QUERY_SIZE) == 0) {
		HandleUserListQuery(ClientIndex);
	}
	else if (strncmp(ReceivedData, "GAME_STATE_QUERY", GAME_STATE_QUERY_SIZE) == 0) { // todo - not in the instruction list !!
		HandleGameStateQuery(ClientIndex); // todo
	}
	else if (strncmp(ReceivedData, "BOARD_VIEW_QUERY", BOARD_VIEW_QUERY_SIZE) == 0) { // todo - not in the instruction list !!
		SendBoardView(ClientIndex, true);
	}
	else {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Got unexpected answer from client. Exiting...\n");
		free(ReceivedData);
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Received from client: %s\n", ReceivedData);
	WriteToLogFile(Server.LogFilePtr, TempMessage);
	free(ReceivedData);
}

void HandlePlayRequest(int ClientIndex, char *ReceivedData) {
	char PlayReply[MESSAGE_LENGTH];
	int RowIndex = ReceivedData[PLAY_REQUEST_SIZE] - '0' - 1; // will give the digit char as int. -1 to make [1-3] -> [0-2]
	int ColumnIndex = ReceivedData[PLAY_REQUEST_SIZE + 2] - '0' - 1;
	bool PlayWasAccepted = false;

	if (Server.Turn != Server.Players[ClientIndex].PlayerType) {
		strcpy(PlayReply, "PLAY_DECLINED:Not your turn\n");
	}
	else if (Server.GameStatus = NotStarted) {
		strcpy(PlayReply, "PLAY_DECLINED:Game has not started\n");
	}
	else if (RowIndex >= BOARD_SIZE || ColumnIndex >= BOARD_SIZE) {
		strcpy(PlayReply, "PLAY_DECLINED:Illegal move\n");
	}
	else if (Server.Board[RowIndex][ColumnIndex] != None) {
		strcpy(PlayReply, "PLAY_DECLINED:Illegal move\n");
	}
	else {
		strcpy(PlayReply, "PLAY_ACCEPTED\n");
		PlayWasAccepted = true;
	}
	int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], PlayReply, Server.LogFilePtr);
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Sent %s to Client %d, UserName %s.\n",
						  PlayReply, ClientIndex, Server.Players[ClientIndex].UserName);
	WriteToLogFile(Server.LogFilePtr, TempMessage);

	if (PlayWasAccepted) { // handle play accepted
		Server.Board[RowIndex][ColumnIndex] = Server.Players[ClientIndex].PlayerType; // mark play
		Server.Turn = (Server.Turn == X) ? O : X; // switch turns
		for (int Client = 0; Client < NUMBER_OF_CLIENTS; Client++) { // send to both BOARD_VIEW and TURN_SWITCH
			SendBoardView(Client, false);
			SendTurnSwitch(Client);
		}
		CheckIfGameEnded();
	}
}

void CheckIfGameEnded() {
	int ORowCounter = 0;
	int XRowCounter = 0;
	int OColumnCounter = 0;
	int XColumnCounter = 0;
	bool OWon = false;
	bool XWon = false;
	bool BoardIsFull = true; // first assuming board is full

	for (int ColumnIndex = 0; ColumnIndex < BOARD_SIZE; ColumnIndex++) { // check row/column win
		for (int RowIndex = 0; RowIndex < BOARD_SIZE; RowIndex++) {
			ORowCounter = (Server.Board[RowIndex][ColumnIndex] == O) ? ORowCounter + 1 : ORowCounter;
			XRowCounter = (Server.Board[RowIndex][ColumnIndex] == X) ? XRowCounter + 1 : XRowCounter;
			OColumnCounter = (Server.Board[ColumnIndex][RowIndex] == O) ? OColumnCounter + 1 : OColumnCounter;
			XColumnCounter = (Server.Board[ColumnIndex][RowIndex] == X) ? XColumnCounter + 1 : XColumnCounter;
			if (Server.Board[RowIndex][ColumnIndex] == None && BoardIsFull) {
				BoardIsFull = false;
			}
		}
		OWon = (ORowCounter == BOARD_SIZE || OColumnCounter == BOARD_SIZE) ? true : OWon;
		XWon = (ORowCounter == BOARD_SIZE || XColumnCounter == BOARD_SIZE) ? true : XWon;
		ORowCounter = 0; // reset for next check
		XRowCounter = 0;
		OColumnCounter = 0;
		XColumnCounter = 0;
		if (OWon || XWon) {
			break;
		}
	}
	for (int RowIndex = 0; RowIndex < BOARD_SIZE; RowIndex++) { // check diagonal win
		ORowCounter = (Server.Board[RowIndex][RowIndex] == O) ? ORowCounter + 1 : ORowCounter;
		XRowCounter = (Server.Board[RowIndex][RowIndex] == X) ? XRowCounter + 1 : XRowCounter;
		OColumnCounter = (Server.Board[RowIndex][BOARD_SIZE - 1 - RowIndex] == O) ? OColumnCounter + 1 : OColumnCounter;
		XColumnCounter = (Server.Board[RowIndex][BOARD_SIZE - 1 - RowIndex] == X) ? XColumnCounter + 1 : XColumnCounter;
	}
	OWon = (ORowCounter == BOARD_SIZE || OColumnCounter == BOARD_SIZE) ? true : OWon;
	XWon = (XRowCounter == BOARD_SIZE || XColumnCounter == BOARD_SIZE) ? true : XWon;
	OWon = true; // todo patch - remove !!
	if (OWon || XWon || BoardIsFull) {
		HandleGameEndedMessage(OWon, XWon, BoardIsFull);
		Server.GameStatus = Ended;
	}
}

void HandleGameEndedMessage(bool OWon, bool XWon, bool BoardIsFull) {
	char GameEndedMessage[MESSAGE_LENGTH];
	if (XWon) {
		sprintf(GameEndedMessage, "GAME_ENDED:%s\n", Server.Players[0].UserName);
	}
	else if (OWon) {
		sprintf(GameEndedMessage, "GAME_ENDED:%s\n", Server.Players[1].UserName);
	}
	else if (BoardIsFull) {
		strcpy(GameEndedMessage, "GAME_ENDED:Tie\n");
	}
	for (int ClientIndex = 0; ClientIndex < NUMBER_OF_CLIENTS; ClientIndex++) {
		int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], GameEndedMessage, Server.LogFilePtr);
		if (SendDataToServerReturnValue == ERROR_CODE) {
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
	}
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Sent %s.\n", GameEndedMessage);
	WriteToLogFile(Server.LogFilePtr, TempMessage);
}

void HandleUserListQuery(int ClientIndex) {
	char UserListReply[MESSAGE_LENGTH];
	if (Server.NumberOfConnectedUsers == 2) {
		sprintf(UserListReply, "USER_LIST_REPLY:Players: x:%s, o:%s\n", Server.Players[0].UserName, Server.Players[1].UserName);
	}
	else {
		sprintf(UserListReply, "USER_LIST_REPLY:Players: x:%s\n", Server.Players[0].UserName);
	}
	int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], UserListReply, Server.LogFilePtr);
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Sent USER_LIST_REPLY to Client %d, UserName %s.\n",
						  ClientIndex, Server.Players[ClientIndex].UserName);
	WriteToLogFile(Server.LogFilePtr, TempMessage);
}

void HandleGameStateQuery(int ClientIndex) { // todo they didn't tell how to send this !!
	char GameStateReply[MESSAGE_LENGTH];
	switch (Server.GameStatus) {
	case NotStarted:
		strcpy(GameStateReply, "GAME_STATE_REPLY:Game has not started\n");
		break;
	case Started:
		
		break;
	}
	int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], GameStateReply, Server.LogFilePtr);
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Sent GAME_STATE_REPLY to Client %d, UserName %s.\n",
						  ClientIndex, Server.Players[ClientIndex].UserName);
	WriteToLogFile(Server.LogFilePtr, TempMessage);
}

void CloseSocketsAndThreads() {
	int ClientIndex = 0;
	for (; ClientIndex < NUMBER_OF_CLIENTS; ClientIndex++) {
		CloseOneSocket(Server.ClientsSockets[ClientIndex], Server.LogFilePtr);
		CloseOneThreadHandle(Server.ClientsThreadHandle[ClientIndex], Server.LogFilePtr);
	}
	CloseOneThreadHandle(Server.NumberOfConnectedUsersMutex, Server.LogFilePtr);
	CloseOneSocket(Server.ListeningSocket, Server.LogFilePtr);
	
	CloseWsaData(Server.LogFilePtr);
}