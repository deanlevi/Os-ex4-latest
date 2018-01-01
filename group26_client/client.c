#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "client.h"

void InitClient(char *argv[]);
void HandleClient();
void ConnectToServer();
void CreateThreadsAndSemaphores();
void WINAPI SendThread();
void WaitForSendToServerSemaphore();
void HandleNewUserRequest();
void WINAPI ReceiveThread();
void HandleReceivedData(char *ReceivedData);
void HandleNewUserAccept(char *ReceivedData);
void HandleBoardView(char *ReceivedData);
void HandleTurnSwitch(char *ReceivedData);
void HandlePlayDeclined(char *ReceivedData);
void HandleGameEnded(char *ReceivedData);
void HandleUserListReply(char *ReceivedData);
void WINAPI UserInterfaceThread();
void WaitForUserInterfaceSemaphore();
void HandleInputFromUser(char *UserInput);
void CloseSocketAndThreads();


void InitClient(char *argv[]) {
	Client.Socket = INVALID_SOCKET;
	Client.LogFilePtr = argv[1];
	Client.ServerIP = argv[2];
	Client.ServerPortNum = atoi(argv[3]);
	Client.UserName = argv[4];

	int ThreadIndex = 0;
	for (; ThreadIndex < NUMBER_OF_THREADS_TO_HANDLE_CLIENT; ThreadIndex++) {
		Client.ThreadHandles[ThreadIndex] = NULL;
	}
	Client.UserInterfaceSemaphore = NULL;
	Client.SendToServerSemaphore = NULL;
	Client.PlayerType = None;
	InitLogFile(Client.LogFilePtr);
}

void HandleClient() {
	DWORD wait_code;

	InitWsaData();
	Client.Socket = CreateOneSocket();
	if (Client.Socket == INVALID_SOCKET) {
		char ErrorMessage[MESSAGE_LENGTH];
		sprintf(ErrorMessage, "Custom message: HandleClient failed to create socket.\nError Number is %d\n", WSAGetLastError());
		WriteToLogFile(Client.LogFilePtr, ErrorMessage);
		exit(ERROR_CODE);
	}

	ConnectToServer();
	CreateThreadsAndSemaphores();

	wait_code = WaitForMultipleObjects(NUMBER_OF_THREADS_TO_HANDLE_CLIENT, Client.ThreadHandles, TRUE, INFINITE); // todo check INFINITE
	if (WAIT_OBJECT_0 != wait_code) {
		WriteToLogFile(Client.LogFilePtr, "Custom message: Error when waiting for program to end.\n");
		CloseSocketAndThreads(); // todo add/check print
		exit(ERROR_CODE);
	}
}

void ConnectToServer() {
	int ConnectReturnValue;
	char ConnectMessage[MESSAGE_LENGTH];

	Client.SocketService.sin_family = AF_INET;
	Client.SocketService.sin_addr.s_addr = inet_addr(Client.ServerIP);
	Client.SocketService.sin_port = htons(Client.ServerPortNum);

	ConnectReturnValue = connect(Client.Socket, (SOCKADDR*)&Client.SocketService, sizeof(Client.SocketService));
	if (ConnectReturnValue == SOCKET_ERROR) {
		sprintf(ConnectMessage, "Custom message: Failed connecting to server on %s:%d. Exiting.\n", Client.ServerIP, Client.ServerPortNum);
		WriteToLogFile(Client.LogFilePtr, ConnectMessage);
		CloseSocketAndThreads(); // todo check if add function to handle error
		exit(ERROR_CODE);
	}
	sprintf(ConnectMessage, "Connected to server on %s:%d.\n", Client.ServerIP, Client.ServerPortNum);
	OutputMessageToWindowAndLogFile(Client.LogFilePtr, ConnectMessage);
}

void CreateThreadsAndSemaphores() {
	Client.ThreadHandles[0] = CreateThreadSimple((LPTHREAD_START_ROUTINE)SendThread,
		NULL,
		&Client.ThreadIDs[0],
		Client.LogFilePtr);

	Client.ThreadHandles[1] = CreateThreadSimple((LPTHREAD_START_ROUTINE)ReceiveThread,
		NULL,
		&Client.ThreadIDs[1],
		Client.LogFilePtr);

	Client.ThreadHandles[2] = CreateThreadSimple((LPTHREAD_START_ROUTINE)UserInterfaceThread,
		NULL,
		&Client.ThreadIDs[2],
		Client.LogFilePtr);

	if (Client.ThreadHandles[0] == NULL || Client.ThreadHandles[1] == NULL || Client.ThreadHandles[2] == NULL) {
		WriteToLogFile(Client.LogFilePtr, "Custom message: CreateThreadsAndSemaphores failed to create threads.\n");
		CloseSocketAndThreads(); // todo check if add function to handle error
		exit(ERROR_CODE);
	}

	Client.UserInterfaceSemaphore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - not signaled */
		1,		/* Maximum Count */
		NULL);	/* un-named */

	if (Client.UserInterfaceSemaphore == NULL) {
		WriteToLogFile(Client.LogFilePtr, "Custom message: CreateThreadsAndSemaphores - Error when creating UserInterface semaphore.\n");
		CloseSocketAndThreads(); // todo check if add function to handle error
		exit(ERROR_CODE);
	}
	Client.SendToServerSemaphore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - not signaled */
		1,		/* Maximum Count */
		NULL);	/* un-named */

	if (Client.SendToServerSemaphore == NULL) {
		WriteToLogFile(Client.LogFilePtr, "Custom message: CreateThreadsAndSemaphores - Error when creating SendToServer semaphore.\n");
		CloseSocketAndThreads(); // todo check if add function to handle error
		exit(ERROR_CODE);
	}
}

void WINAPI SendThread() {
	int SendDataToServerReturnValue;
	HandleNewUserRequest();
	while (TRUE) {
		WaitForSendToServerSemaphore(); // todo check if add mutex
		SendDataToServerReturnValue = SendData(Client.Socket, Client.MessageToSendToServer, Client.LogFilePtr);
		if (SendDataToServerReturnValue == ERROR_CODE) {
			CloseSocketAndThreads();
			exit(ERROR_CODE);
		}
		char TempMessage[MESSAGE_LENGTH];
		sprintf(TempMessage, "Sent to server: %s\n", Client.MessageToSendToServer);
		WriteToLogFile(Client.LogFilePtr, TempMessage);
	}
}

void WaitForSendToServerSemaphore() {
	DWORD wait_code;

	wait_code = WaitForSingleObject(Client.SendToServerSemaphore, INFINITE); // wait for send to server notification
	if (WAIT_OBJECT_0 != wait_code) {
		WriteToLogFile(Client.LogFilePtr, "Custom message: SendThread - failed to wait for  UserInterface semaphore.\n");
		CloseSocketAndThreads(); // todo check if add function to handle error
		exit(ERROR_CODE);
	}
}

void HandleNewUserRequest() {
	char NewUserRequest[MESSAGE_LENGTH];
	int SendDataToServerReturnValue;
	sprintf(NewUserRequest, "NEW_USER_REQUEST:%s\n", Client.UserName);
	SendDataToServerReturnValue = SendData(Client.Socket, NewUserRequest, Client.LogFilePtr); // todo check if add mutex
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Sent to server: %s\n", NewUserRequest);
	WriteToLogFile(Client.LogFilePtr, TempMessage);
}

void WINAPI ReceiveThread() {
	while (TRUE) {
		char *ReceivedData = ReceiveData(Client.Socket, Client.LogFilePtr);
		if (ReceivedData == NULL) {
			CloseSocketAndThreads();
			exit(ERROR_CODE);
		}
		HandleReceivedData(ReceivedData);
	}
}

void HandleReceivedData(char *ReceivedData) {
	char TempMessage[MESSAGE_LENGTH];
	if (strncmp(ReceivedData, "NEW_USER_ACCEPTED", NEW_USER_ACCEPTED_OR_DECLINED_SIZE) == 0 ||
		strncmp(ReceivedData, "NEW_USER_DECLINED", NEW_USER_ACCEPTED_OR_DECLINED_SIZE) == 0) {
		HandleNewUserAccept(ReceivedData);
	}
	else if (strncmp(ReceivedData, "GAME_STARTED", GAME_STARTED_SIZE) == 0) {
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, "Game is on!\n");
	}
	else if (strncmp(ReceivedData, "BOARD_VIEW:", BOARD_VIEW_SIZE) == 0) {
		HandleBoardView(ReceivedData);
	}
	else if (strncmp(ReceivedData, "TURN_SWITCH:", TURN_SWITCH_SIZE) == 0) {
		HandleTurnSwitch(ReceivedData);
	}
	else if (strncmp(ReceivedData, "PLAY_ACCEPTED", PLAY_ACCEPTED_SIZE) == 0) {
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, "Well played\n");
	}
	else if (strncmp(ReceivedData, "PLAY_DECLINED:", PLAY_DECLINED_SIZE) == 0) {
		HandlePlayDeclined(ReceivedData);
	}
	else if (strncmp(ReceivedData, "GAME_ENDED:", GAME_ENDED_SIZE) == 0) {
		HandleGameEnded(ReceivedData); // todo
	}
	else if (strncmp(ReceivedData, "USER_LIST_REPLY:", USER_LIST_REPLY_SIZE) == 0) {
		HandleUserListReply(ReceivedData); // todo
	}
	else if (strncmp(ReceivedData, "GAME_STATE_REPLY:", GAME_STATE_REPLY_SIZE) == 0) { // todo check - not in instruction list !!
		// todo - same as TURN_SWITCH ?
	}
	else {
		sprintf(TempMessage, "Custom message: Got unexpected answer from Server:\n%s.\nExiting...\n", ReceivedData);
		WriteToLogFile(Client.LogFilePtr, TempMessage);
		free(ReceivedData);
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
	sprintf(TempMessage, "Received from server: %s\n", ReceivedData);
	WriteToLogFile(Client.LogFilePtr, TempMessage);
	free(ReceivedData);
}

void HandleNewUserAccept(char *ReceivedData) {
	if (strncmp(ReceivedData, "NEW_USER_DECLINED", NEW_USER_ACCEPTED_OR_DECLINED_SIZE) == 0) {
		WriteToLogFile(Client.LogFilePtr, "Request to join was refused.\n");
		free(ReceivedData);
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
	if (strncmp(ReceivedData + NEW_USER_ACCEPTED_OR_DECLINED_SIZE + 1, "x", 1) == 0) {
		Client.PlayerType = X;
	} else if (strncmp(ReceivedData + NEW_USER_ACCEPTED_OR_DECLINED_SIZE + 1, "o", 1) == 0) {
		Client.PlayerType = O;
	}
	else {
		WriteToLogFile(Client.LogFilePtr, "Custom message: Got unexpected answer from Server. Exiting...\n");
		free(ReceivedData);
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
	if (ReleaseOneSemaphore(Client.UserInterfaceSemaphore) == FALSE) { // to release UserInterface thread
		WriteToLogFile(Client.LogFilePtr, "Custom message: SendThread - failed to release receive semaphore.\n");
		CloseSocketAndThreads(); // todo check if add function to handle error
		exit(ERROR_CODE);
	}
	// todo check if need to parse player number. 1/2.
}

void HandleBoardView(char *ReceivedData) {
	char BoardMessage[MESSAGE_LENGTH];
	int LineLength = 7; // size of "| | | |"
	int LineIndex = 0;
	strncpy(BoardMessage, ReceivedData + BOARD_MESSAGE_LINE_OFFSET, LineLength);
	BoardMessage[LineLength] = '\0';
	strncat(BoardMessage, "\n", 1);
	strncat(BoardMessage, ReceivedData + BOARD_MESSAGE_LINE_LENGTH + BOARD_MESSAGE_LINE_OFFSET, LineLength);
	BoardMessage[2*LineLength + 1] = '\0';
	strncat(BoardMessage, "\n", 1);
	strncat(BoardMessage, ReceivedData + 2*BOARD_MESSAGE_LINE_LENGTH + BOARD_MESSAGE_LINE_OFFSET, LineLength);
	BoardMessage[2*(LineLength + 1) + LineLength] = '\0';
	strncat(BoardMessage, "\n", 1);

	printf("%s", BoardMessage);
}

void HandleTurnSwitch(char *ReceivedData) {
	char TurnSwitchMessage[MESSAGE_LENGTH];
	char UserName[USER_NAME_LENGTH];
	int UserNameStartPosition = TURN_SWITCH_SIZE;
	int UserNameEndPosition = UserNameStartPosition;
	while (ReceivedData[UserNameEndPosition] != ';') {
		UserNameEndPosition++;
	}
	int UserNameLength = UserNameEndPosition - UserNameStartPosition;
	strncpy(UserName, ReceivedData + UserNameStartPosition, UserNameLength);
	UserName[UserNameLength] = '\0';
	char Symbol = ReceivedData[UserNameEndPosition + 1];
	sprintf(TurnSwitchMessage, "%s's turn (%c)\n", UserName, Symbol);
	OutputMessageToWindowAndLogFile(Client.LogFilePtr, TurnSwitchMessage);
}

void HandlePlayDeclined(char *ReceivedData) {
	int EndOfMessageOffset = PLAY_DECLINED_SIZE;
	char PlayerDeclinedMessage[MESSAGE_LENGTH];
	strcpy(PlayerDeclinedMessage, "Error: ");
	while (ReceivedData[EndOfMessageOffset] != '\n') {
		EndOfMessageOffset++;
	}
	strncat(PlayerDeclinedMessage, ReceivedData + PLAY_DECLINED_SIZE, (EndOfMessageOffset - PLAY_DECLINED_SIZE));
	OutputMessageToWindowAndLogFile(Client.LogFilePtr, PlayerDeclinedMessage);
}

void HandleGameEnded(char *ReceivedData) {
	if (strncmp(ReceivedData + GAME_ENDED_SIZE, "Tie", 3) == 0) { // todo verify structure
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, "Game ended. Everybody wins!\n");
	}
	else {
		char GameEndedMessage[MESSAGE_LENGTH];
		char UserName[USER_NAME_LENGTH];
		int ParameterEndPosition = GAME_ENDED_SIZE;
		while (ReceivedData[ParameterEndPosition] != '\n') {
			ParameterEndPosition++;
		}
		strncpy(UserName, ReceivedData + GAME_ENDED_SIZE, ParameterEndPosition - GAME_ENDED_SIZE);
		sprintf(GameEndedMessage, "Game ended. The winner is %s!", UserName);
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, GameEndedMessage);
	}
	// todo handle close connection
}

void HandleUserListReply(char *ReceivedData) {
	char UserListReply[MESSAGE_LENGTH];
	int UserListReplyEndPosition = USER_LIST_REPLY_SIZE;
	while (ReceivedData[UserListReplyEndPosition] != '\n') {
		UserListReplyEndPosition++; // todo make a method that finds the first '\n' and add to general methods
	}
	strncpy(UserListReply, ReceivedData + USER_LIST_REPLY_SIZE, UserListReplyEndPosition - USER_LIST_REPLY_SIZE);
	int TotalReplySize = UserListReplyEndPosition - USER_LIST_REPLY_SIZE;
	UserListReply[TotalReplySize] = '\0';
	strcat(UserListReply, "\n");
	OutputMessageToWindowAndLogFile(Client.LogFilePtr, UserListReply);
}

void WINAPI UserInterfaceThread() {
	WaitForUserInterfaceSemaphore();
	char UserInput[USER_INPUT_LENGTH];
	while (TRUE) {
		scanf("%s", UserInput);
		HandleInputFromUser(UserInput);
	}
}

void WaitForUserInterfaceSemaphore() {
	DWORD wait_code;

	wait_code = WaitForSingleObject(Client.UserInterfaceSemaphore, INFINITE); // wait for connection to be established and user accepted
	if (WAIT_OBJECT_0 != wait_code) {
		WriteToLogFile(Client.LogFilePtr, "Custom message: SendThread - failed to wait for  UserInterface semaphore.\n");
		CloseSocketAndThreads(); // todo check if add function to handle error
		exit(ERROR_CODE);
	}
}

void HandleInputFromUser(char *UserInput) {
	bool NeedToReleaseSendToServerSemaphore = true;
	if (strcmp(UserInput, "players") == 0) {
		strcpy(Client.MessageToSendToServer, "USER_LIST_QUERY\n");
	}
	else if (strcmp(UserInput, "state") == 0) {
		strcpy(Client.MessageToSendToServer, "GAME_STATE_QUERY\n");
	}
	else if (strcmp(UserInput, "board") == 0) {
		strcpy(Client.MessageToSendToServer, "BOARD_VIEW_QUERY\n");
	}
	else if (strcmp(UserInput, "play") == 0) {
		scanf("%s", UserInput); // get first index
		int RowNum = atoi(UserInput);
		scanf("%s", UserInput); // get second index
		int ColumnNum = atoi(UserInput);
		sprintf(Client.MessageToSendToServer, "PLAY_REQUEST:%d;%d\n", RowNum, ColumnNum);
	}
	else if (strcmp(UserInput, "exit") == 0) {
		// todo
		NeedToReleaseSendToServerSemaphore = false; // todo remove when fixing
	}
	else {
		// todo
		NeedToReleaseSendToServerSemaphore = false;
	}
	if (NeedToReleaseSendToServerSemaphore) {
		if (ReleaseOneSemaphore(Client.SendToServerSemaphore) == FALSE) { // signal sending thread that a message is ready for sending
			WriteToLogFile(Client.LogFilePtr, "Custom message: SendThread - failed to release SendToServer semaphore.\n");
			CloseSocketAndThreads(); // todo check if add function to handle error
			exit(ERROR_CODE);
		}
	}
}

void CloseSocketAndThreads() { // todo
	CloseOneSocket(Client.Socket, Client.LogFilePtr);
	int ThreadIndex = 0;
	for (; ThreadIndex < NUMBER_OF_THREADS_TO_HANDLE_CLIENT; ThreadIndex++) {
		CloseOneThreadHandle(Client.ThreadHandles[ThreadIndex], Client.LogFilePtr);
	}
	CloseOneThreadHandle(Client.UserInterfaceSemaphore, Client.LogFilePtr);
	CloseOneThreadHandle(Client.SendToServerSemaphore, Client.LogFilePtr);
	CloseWsaData(Client.LogFilePtr);
}