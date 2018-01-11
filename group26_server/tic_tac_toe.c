/*
Author - Dean Levi 302326640
Project - Ex4
Using - tic_tac_toe.h
Description - implementation of all in-game related operations of the server - game management and clients handling.
*/
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>

#include "tic_tac_toe.h"

/*
Parameters - lpParam: pointer to ClientIndex.
Returns - none.
Description - handles the entire operation of the server during the game.
*/
void WINAPI TicTacToeGameThread(LPVOID lpParam);

/*
Parameters - ClientIndex: the index of the client.
Returns - true if the new user request was accepted and false if declined.
Description - check if new user is accepted and send reply to client.
*/
bool HandleNewUserRequestAndAccept(int ClientIndex);

/*
Parameters - ClientIndex: the index of the client, ReceivedData: the request that was received from client.
Returns - none.
Description - parse new user request sent from client.
*/
void ParseNewUserRequest(char *ReceivedData, int ClientIndex);

/*
Parameters - none.
Returns - none.
Description - when new user is accepted update the number of connected users.
			  if two users are connected signal that game has started.
*/
void UpdateNumberOfConnectedUsers();

/*
Parameters - ClientIndex: the index of the client.
Returns - none.
Description - when game starts - send game started, board view, and turn switch messages to the client.
*/
void SendGameStartedBoardViewAndTurnSwitch(int ClientIndex);

/*
Parameters - ClientIndex: the index of the client, IsBoardViewQuery: if need to send board view query or board view.
Returns - none.
Description - sends to client board view query if IsBoardViewQuery = true, else sends board view.
*/
void SendBoardView(int ClientIndex, bool IsBoardViewQuery);

/*
Parameters - ClientIndex: the index of the client, IsTurnSwitch: if need to send turn switch or game state reply.
Returns - none.
Description - sends to client turn switch if IsTurnSwitch = true, else sends game state reply.
*/
void SendTurn(int ClientIndex, bool IsTurnSwitch);

/*
Parameters - ClientIndex: the index of the client, ReceivedData: the data that was received from client.
Returns - none.
Description - checks the request type received from the client and handles it accordingly.
*/
void HandleReceivedData(char *ReceivedData, int ClientIndex);

/*
Parameters - ClientIndex: the index of the client, ReceivedData: the data that was received from client.
Returns - none.
Description - handles play request from client.
*/
void HandlePlayRequest(int ClientIndex, char *ReceivedData);

/*
Parameters - none.
Returns - none.
Description - checks if game ended and updates server accordingly.
*/
void CheckIfGameEnded();

/*
Parameters - OWon: signals if O won, XWon: signals if X won, BoardIsFull: signals if board is full and it's a tie.
Returns - none.
Description - when game is ended, handles the game ended message to send to clients according to game's result.
*/
void HandleGameEndedMessage(bool OWon, bool XWon, bool BoardIsFull);

/*
Parameters - ClientIndex: the index of the client.
Returns - none.
Description - handle user list reply message to send to client.
*/
void HandleUserListQuery(int ClientIndex);

void WINAPI TicTacToeGameThread(LPVOID lpParam) {
	if (NULL == lpParam) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error in TicTacToeGame. Received null pointer.\n");
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	DWORD wait_code;
	BOOL ret_val;
	int *ClientIndexPtr = (int*)lpParam;
	bool UserWasAccepted = HandleNewUserRequestAndAccept(*ClientIndexPtr);
	if (!UserWasAccepted) {
		if (ReleaseOneSemaphore(Server.NumberOfConnectedUsersSemaphore) == FALSE) { // to signal to ConnectUsersThread
			WriteToLogFile(Server.LogFilePtr, "Custom message: failed to release NumberOfConnectedUsers semaphore.\n");
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		return;
	}
	UpdateNumberOfConnectedUsers();
	SendGameStartedBoardViewAndTurnSwitch(*ClientIndexPtr);

	while (TRUE) { // game started
		char *ReceivedData = ReceiveData(Server.ClientsSockets[*ClientIndexPtr], Server.LogFilePtr);
		if (ReceivedData == NULL) {
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		if (strcmp(ReceivedData, "FINISHED") == 0) {
			Server.NumberOfConnectedUsers -= 1;
			CloseOneSocket(Server.ClientsSockets[*ClientIndexPtr], Server.LogFilePtr); // close client socket
			if (Server.GameStatus == NotStarted) {
				Server.FirstClientDisconnectedBeforeGameStarted = true;
				if (TerminateThread(Server.ConnectUsersThreadHandle, WAIT_OBJECT_0) == FALSE) { // reset for new connect session
					WriteToLogFile(Server.LogFilePtr, "Custom message: Error when terminating ConnectUsersThreadHandle.\n");
					CloseSocketsAndThreads();
					exit(ERROR_CODE);
				}
			}
			break; // finished communication
		}
		if (Server.NumberOfConnectedUsers < NUMBER_OF_CLIENTS && Server.GameStatus == Started) {
			OutputMessageToWindowAndLogFile(Server.LogFilePtr, "Player disconnected. Exiting.\n");
			shutdown(Server.ClientsSockets[*ClientIndexPtr], SD_BOTH);
			CloseOneSocket(Server.ClientsSockets[*ClientIndexPtr], Server.LogFilePtr); // close client socket
			Server.GameStatus = Ended;
			if (TerminateThread(Server.ConnectUsersThreadHandle, WAIT_OBJECT_0) == FALSE) { // reset for next game
				WriteToLogFile(Server.LogFilePtr, "Custom message: Error when terminating ConnectUsersThreadHandle.\n");
				CloseSocketsAndThreads();
				exit(ERROR_CODE);
			}
			break;
		}
		wait_code = WaitForSingleObject(Server.ServerPropertiesUpdatesMutex, INFINITE); // wait for Mutex access
		if (WAIT_OBJECT_0 != wait_code) {
			WriteToLogFile(Server.LogFilePtr, "Custom message: Error in wait for ServerPropertiesUpdatesMutex.\n");
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		HandleReceivedData(ReceivedData, *ClientIndexPtr); // each time one client is handled
		Sleep(SEND_MESSAGES_WAIT);
		ret_val = ReleaseMutex(Server.ServerPropertiesUpdatesMutex); // release mutex
		if (FALSE == ret_val) {
			WriteToLogFile(Server.LogFilePtr, "Custom message: Error in releasing ServerPropertiesUpdatesMutex.\n");
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
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
	char NewUserReply[MESSAGE_LENGTH];
	bool UserNameIsTaken = (ClientIndex == 1) && (strcmp(Server.Players[0].UserName, Server.Players[1].UserName) == 0);
	bool UserAccepted;
	if (UserNameIsTaken) {
		strcpy(NewUserReply, "NEW_USER_DECLINED\n");
		UserAccepted = false;
	}
	else {
		Server.Players[ClientIndex].PlayerType = ClientIndex == 0 ? X : O;
		if (Server.Players[ClientIndex].PlayerType == X) {
			sprintf(NewUserReply, "NEW_USER_ACCEPTED:x;1\n");
		}
		else {
			sprintf(NewUserReply, "NEW_USER_ACCEPTED:o;2\n");
		}
		UserAccepted = true;
	}
	int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], NewUserReply, Server.LogFilePtr);
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	Sleep(SEND_MESSAGES_WAIT); // give a little time before sending game is on
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Sent %s to Client %d, UserName %s.\n",
						  NewUserReply, ClientIndex, Server.Players[ClientIndex].UserName);
	WriteToLogFile(Server.LogFilePtr, TempMessage);
	return UserAccepted;
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
	EndPosition = FindEndOfDataSymbol(ReceivedData);
	ParameterSize = (EndPosition - 1) - StartPosition + 1;
	strncpy(Server.Players[ClientIndex].UserName, ReceivedData + StartPosition, ParameterSize);
	Server.Players[ClientIndex].UserName[ParameterSize] = '\0';
	free(ReceivedData);
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Received NEW_USER_REQUEST from Client %d, UserName %s.\n",
						  ClientIndex, Server.Players[ClientIndex].UserName);
	WriteToLogFile(Server.LogFilePtr, TempMessage);
}

void UpdateNumberOfConnectedUsers() {
	DWORD wait_code;
	BOOL ret_val;

	wait_code = WaitForSingleObject(Server.ServerPropertiesUpdatesMutex, INFINITE); // wait for Mutex access
	if (WAIT_OBJECT_0 != wait_code) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error in wait for ServerPropertiesUpdatesMutex.\n");
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	Server.NumberOfConnectedUsers++;
	if (Server.NumberOfConnectedUsers == 2) {
		Server.GameStatus = Started;
	}
	if (ReleaseOneSemaphore(Server.NumberOfConnectedUsersSemaphore) == FALSE) { // to signal to ConnectUsersThread
		WriteToLogFile(Server.LogFilePtr, "Custom message: failed to release NumberOfConnectedUsers semaphore.\n");
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	ret_val = ReleaseMutex(Server.ServerPropertiesUpdatesMutex); // release mutex
	if (FALSE == ret_val) {
		WriteToLogFile(Server.LogFilePtr, "Custom message: Error in releasing ServerPropertiesUpdatesMutex.\n");
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
}

void SendGameStartedBoardViewAndTurnSwitch(int ClientIndex) {
	int SendDataToServerReturnValue = SendData(Server.ClientsSockets[ClientIndex], "GAME_STARTED\n", Server.LogFilePtr);
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketsAndThreads();
		exit(ERROR_CODE);
	}
	Sleep(SEND_MESSAGES_WAIT);
	SendBoardView(ClientIndex, false);
	Sleep(SEND_MESSAGES_WAIT);
	SendTurn(ClientIndex, true);
}

void SendBoardView(int ClientIndex, bool IsBoardViewQuery) {
	char BoardMessage[MESSAGE_LENGTH];
	int LineOffset, LineLength;
	char CharToUpdate;
	if (IsBoardViewQuery) {
		strcpy(BoardMessage, "BOARD_VIEW_REPLY:| | | |");
		strcat(BoardMessage, "                 | | | |");
		strcat(BoardMessage, "                 | | | |\n");
		LineOffset = BOARD_VIEW_QUERY_SIZE + 1;
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

void SendTurn(int ClientIndex, bool IsTurnSwitch) {
	char TurnMessage[MESSAGE_LENGTH];
	char *MessageType = (IsTurnSwitch) ? "TURN_SWITCH" : "GAME_STATE_REPLY";
	if (Server.GameStatus == NotStarted && !IsTurnSwitch) {
		sprintf(TurnMessage, "%s:Game has not started\n", MessageType);
	}
	else if (Server.GameStatus == Ended && !IsTurnSwitch) {
		sprintf(TurnMessage, "%s:Error: Game has already ended\n", MessageType);
	}
	else {
		char TypeOfPlayerWhoHasTurn = Server.Turn == X ? 'x' : 'o';
		char *UserNameOfPlayerWhoHasTurn = Server.Turn == X ? Server.Players[0].UserName : Server.Players[1].UserName;
		sprintf(TurnMessage, "%s:%s;%c\n", MessageType, UserNameOfPlayerWhoHasTurn, TypeOfPlayerWhoHasTurn);
	}
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
		HandlePlayRequest(ClientIndex, ReceivedData);
	}
	else if (strncmp(ReceivedData, "USER_LIST_QUERY", USER_LIST_QUERY_SIZE) == 0) {
		HandleUserListQuery(ClientIndex);
	}
	else if (strncmp(ReceivedData, "GAME_STATE_QUERY", GAME_STATE_QUERY_SIZE) == 0) {
		SendTurn(ClientIndex, false);
	}
	else if (strncmp(ReceivedData, "BOARD_VIEW_QUERY", BOARD_VIEW_QUERY_SIZE) == 0) {
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
	else if (Server.GameStatus == NotStarted) {
		strcpy(PlayReply, "PLAY_DECLINED:Game has not started\n");
	}
	else if (RowIndex >= BOARD_SIZE || ColumnIndex >= BOARD_SIZE || Server.Board[RowIndex][ColumnIndex] != None) {
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
			Sleep(SEND_MESSAGES_WAIT);
			SendTurn(Client, true);
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
		XWon = (XRowCounter == BOARD_SIZE || XColumnCounter == BOARD_SIZE) ? true : XWon;
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
	if (OWon || XWon || BoardIsFull) {
		Sleep(SEND_MESSAGES_WAIT); // let turn message pass
		HandleGameEndedMessage(OWon, XWon, BoardIsFull);
		Server.GameStatus = Ended;
		if (TerminateThread(Server.ConnectUsersThreadHandle, WAIT_OBJECT_0) == FALSE) { // reset for next game
			WriteToLogFile(Server.LogFilePtr, "Custom message: Error when terminating ConnectUsersThreadHandle.\n");
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
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
	Sleep(SEND_MESSAGES_WAIT); // so server will send game ended message before closing connections
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Custom message: Sent %s.\n", GameEndedMessage);
	WriteToLogFile(Server.LogFilePtr, TempMessage);
}

void HandleUserListQuery(int ClientIndex) {
	char UserListReply[MESSAGE_LENGTH];
	if (Server.NumberOfConnectedUsers == 2) {
		//sprintf(UserListReply, "USER_LIST_REPLY:Players: x:%s, o:%s\n", Server.Players[0].UserName, Server.Players[1].UserName);
		sprintf(UserListReply, "USER_LIST_REPLY:x;%s;o;%s\n", Server.Players[0].UserName, Server.Players[1].UserName);
	}
	else {
		//sprintf(UserListReply, "USER_LIST_REPLY:Players: x:%s\n", Server.Players[0].UserName);
		sprintf(UserListReply, "USER_LIST_REPLY:x;%s\n", Server.Players[0].UserName);
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