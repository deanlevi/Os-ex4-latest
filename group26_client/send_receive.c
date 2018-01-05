#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#include "send_receive.h"

void WINAPI SendThread();
void WaitForSendToServerSemaphore();
void HandleNewUserRequest();
void WINAPI ReceiveThread();
void HandleReceivedData(char *ReceivedData);
void HandleNewUserAccept(char *ReceivedData);
void HandleBoardView(char *ReceivedData, bool IsBoardViewQuery);
void HandleTurn(char *ReceivedData, bool IsTurnSwitch);
void HandlePlayDeclined(char *ReceivedData);
void HandleGameEnded(char *ReceivedData);
void HandleUserListReply(char *ReceivedData);
void FindNextParameterEnd(char *ReceivedData, bool *ReachedEndOfData, int *ParameterEndPosition);

void WINAPI SendThread() {
	int SendDataToServerReturnValue;
	HandleNewUserRequest();
	while (TRUE) {
		WaitForSendToServerSemaphore();
		if (strcmp(Client.MessageToSendToServer, "FINISHED") == 0) {
			break; // finished communication
		}
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
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
}

void HandleNewUserRequest() {
	char NewUserRequest[MESSAGE_LENGTH];
	int SendDataToServerReturnValue;
	sprintf(NewUserRequest, "NEW_USER_REQUEST:%s\n", Client.UserName);
	SendDataToServerReturnValue = SendData(Client.Socket, NewUserRequest, Client.LogFilePtr);
	if (SendDataToServerReturnValue == ERROR_CODE) {
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
	char TempMessage[MESSAGE_LENGTH];
	sprintf(TempMessage, "Sent to server: %s\n", NewUserRequest);
	WriteToLogFile(Client.LogFilePtr, TempMessage);
}

void WINAPI ReceiveThread() {
	char *ReceivedData = NULL;
	while (TRUE) {
		if (!Client.GotExitFromUserOrGameFinished) {
			ReceivedData = ReceiveData(Client.Socket, Client.LogFilePtr);
			if (ReceivedData == NULL) {
				CloseSocketAndThreads();
				exit(ERROR_CODE);
			}
		};
		if (Client.GotExitFromUserOrGameFinished || strcmp(ReceivedData, "FINISHED") == 0) {
			if (Client.GotExitFromUserOrGameFinished) {
				strcpy(Client.MessageToSendToServer, "FINISHED");
				if (ReleaseOneSemaphore(Client.SendToServerSemaphore) == FALSE) { // signal sending thread to finish
					WriteToLogFile(Client.LogFilePtr, "Custom message: ReceiveThread - failed to release SendToServer semaphore.\n");
					CloseSocketAndThreads();
					exit(ERROR_CODE); // todo exits like this when game ends because server disconnects
				}
				if (Client.ThreadHandles[2] != NULL) { // if user interface is active
					if (TerminateThread(Client.ThreadHandles[2], WAIT_OBJECT_0) == FALSE) {
						WriteToLogFile(Client.LogFilePtr, "Custom message: ReceiveThread - failed to terminate user interface thread.\n");
						CloseSocketAndThreads();
						exit(ERROR_CODE);
					}
				}
				break;
			}
			else { // server disconnected - need to exit now
				OutputMessageToWindowAndLogFile(Client.LogFilePtr, "Server disconnected. Exiting.\n");
				CloseSocketAndThreads();
				exit(ERROR_CODE);
			}
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
		HandleBoardView(ReceivedData, false);
	}
	else if (strncmp(ReceivedData, "BOARD_VIEW_REPLY:", BOARD_VIEW_REPLY_LINE_OFFSET) == 0) {
		HandleBoardView(ReceivedData, true);
	}
	else if (strncmp(ReceivedData, "TURN_SWITCH:", TURN_SWITCH_SIZE) == 0) {
		HandleTurn(ReceivedData, true);
	}
	else if (strncmp(ReceivedData, "PLAY_ACCEPTED", PLAY_ACCEPTED_SIZE) == 0) {
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, "Well played\n");
	}
	else if (strncmp(ReceivedData, "PLAY_DECLINED:", PLAY_DECLINED_SIZE) == 0) {
		HandlePlayDeclined(ReceivedData);
	}
	else if (strncmp(ReceivedData, "GAME_ENDED:", GAME_ENDED_SIZE) == 0) {
		HandleGameEnded(ReceivedData);
	}
	else if (strncmp(ReceivedData, "USER_LIST_REPLY:", USER_LIST_REPLY_SIZE) == 0) {
		HandleUserListReply(ReceivedData);
	}
	else if (strncmp(ReceivedData, "GAME_STATE_REPLY:", GAME_STATE_REPLY_SIZE) == 0) { // todo check - not in instruction list !!
																					   // todo - same as TURN_SWITCH ?
		HandleTurn(ReceivedData, false);
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
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, "Request to join was refused.\n");
		free(ReceivedData);
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
	if (strncmp(ReceivedData + NEW_USER_ACCEPTED_OR_DECLINED_SIZE + 1, "x", 1) == 0) {
		Client.PlayerType = X;
	}
	else if (strncmp(ReceivedData + NEW_USER_ACCEPTED_OR_DECLINED_SIZE + 1, "o", 1) == 0) {
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
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
}

void HandleBoardView(char *ReceivedData, bool IsBoardViewQuery) {
	char BoardMessage[MESSAGE_LENGTH];
	int OutputLineLength = 7; // size of "| | | |"
	int InputLineLength = IsBoardViewQuery ? BOARD_VIEW_QUERY_LINE_LENGTH : BOARD_VIEW_LINE_LENGTH;
	int InputLineOffset = IsBoardViewQuery ? BOARD_VIEW_REPLY_LINE_OFFSET : BOARD_VIEW_LINE_OFFSET;
	int LineIndex = 0;
	strncpy(BoardMessage, ReceivedData + InputLineOffset, OutputLineLength);
	BoardMessage[OutputLineLength] = '\0';
	strncat(BoardMessage, "\n", 1);
	strncat(BoardMessage, ReceivedData + InputLineLength + InputLineOffset, OutputLineLength);
	BoardMessage[2 * OutputLineLength + 1] = '\0';
	strncat(BoardMessage, "\n", 1);
	strncat(BoardMessage, ReceivedData + 2 * InputLineLength + InputLineOffset, OutputLineLength);
	BoardMessage[2 * (OutputLineLength + 1) + OutputLineLength] = '\0';
	strncat(BoardMessage, "\n", 1);

	printf("%s", BoardMessage);
}

void HandleTurn(char *ReceivedData, bool IsTurnSwitch) {
	char TurnMessage[MESSAGE_LENGTH];
	char FirstParameter[USER_NAME_LENGTH];
	int MessageStartPosition = (IsTurnSwitch) ? TURN_SWITCH_SIZE : GAME_STATE_REPLY_SIZE;
	
	if (strncmp(ReceivedData + MessageStartPosition, "Error: ", 7) == 0) {
		int EndPosition = FindEndOfDataSymbol(ReceivedData);
		strncpy(TurnMessage, ReceivedData + MessageStartPosition, EndPosition - MessageStartPosition);
		TurnMessage[EndPosition - MessageStartPosition] = '\0';
		strcat(TurnMessage, "\n");
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, TurnMessage);
	}
	else {
		int MessageEndPosition = MessageStartPosition;
		while (ReceivedData[MessageEndPosition] != ';' && ReceivedData[MessageEndPosition] != '\n') {
			MessageEndPosition++;
		}
		int MessageLength = MessageEndPosition - MessageStartPosition;
		strncpy(FirstParameter, ReceivedData + MessageStartPosition, MessageLength);
		FirstParameter[MessageLength] = '\0';
		if (ReceivedData[MessageEndPosition] == ';') {
			char Symbol = ReceivedData[MessageEndPosition + 1];
			sprintf(TurnMessage, "%s's turn (%c)\n", FirstParameter, Symbol);
		}
		else {
			sprintf(TurnMessage, "%s\n", FirstParameter);
		}
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, TurnMessage);
	}
}

void HandlePlayDeclined(char *ReceivedData) {
	int EndOfMessageOffset = FindEndOfDataSymbol(ReceivedData);
	char PlayerDeclinedMessage[MESSAGE_LENGTH];
	strcpy(PlayerDeclinedMessage, "Error: ");
	strncat(PlayerDeclinedMessage, ReceivedData + PLAY_DECLINED_SIZE, (EndOfMessageOffset - PLAY_DECLINED_SIZE));
	strcat(PlayerDeclinedMessage, "\n");
	OutputMessageToWindowAndLogFile(Client.LogFilePtr, PlayerDeclinedMessage);
}

void HandleGameEnded(char *ReceivedData) {
	if (strncmp(ReceivedData + GAME_ENDED_SIZE, "Tie", 3) == 0) {
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, "Game ended. Everybody wins!\n");
	}
	else {
		char GameEndedMessage[MESSAGE_LENGTH];
		char UserName[USER_NAME_LENGTH];
		int ParameterEndPosition = FindEndOfDataSymbol(ReceivedData);
		strncpy(UserName, ReceivedData + GAME_ENDED_SIZE, ParameterEndPosition - GAME_ENDED_SIZE);
		UserName[ParameterEndPosition - GAME_ENDED_SIZE] = '\0';
		sprintf(GameEndedMessage, "Game ended. The winner is %s!\n", UserName);
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, GameEndedMessage);
	}
	Client.GotExitFromUserOrGameFinished = true;
	shutdown(Client.Socket, SD_BOTH);
}

void HandleUserListReply(char *ReceivedData) {
	char UserListReply[MESSAGE_LENGTH] = "Players: ";
	int IndexInUserListReply = 9; // size of "Players: "
	int ParameterStartPosition = USER_LIST_REPLY_SIZE;
	bool ReachedEndOfData = false;
	int ParameterEndPosition = ParameterStartPosition;
	UserListReply[IndexInUserListReply] = ReceivedData[ParameterStartPosition]; // get first symbol
	IndexInUserListReply++;
	UserListReply[IndexInUserListReply] = ':';
	IndexInUserListReply++;
	UserListReply[IndexInUserListReply] = '\0';
	ParameterEndPosition += 2;
	ParameterStartPosition = ParameterEndPosition;

	FindNextParameterEnd(ReceivedData, &ReachedEndOfData, &ParameterEndPosition);
	strncat(UserListReply, ReceivedData + ParameterStartPosition, ParameterEndPosition - ParameterStartPosition); // get first username
	if (!ReachedEndOfData) {
		strcat(UserListReply, ", ");
		IndexInUserListReply += (ParameterEndPosition - ParameterStartPosition + 2);
		ParameterEndPosition++;
		UserListReply[IndexInUserListReply] = ReceivedData[ParameterEndPosition]; // get second symbol
		IndexInUserListReply++;
		UserListReply[IndexInUserListReply] = ':';
		IndexInUserListReply++;
		UserListReply[IndexInUserListReply] = '\0';
		ParameterEndPosition += 2;
		ParameterStartPosition = ParameterEndPosition;
		FindNextParameterEnd(ReceivedData, &ReachedEndOfData, &ParameterEndPosition);
		strncat(UserListReply, ReceivedData + ParameterStartPosition, ParameterEndPosition - ParameterStartPosition); // get second username
	}
	strcat(UserListReply, "\n");
	OutputMessageToWindowAndLogFile(Client.LogFilePtr, UserListReply);
}

void FindNextParameterEnd(char *ReceivedData, bool *ReachedEndOfData, int *ParameterEndPosition) {
	while (ReceivedData[*ParameterEndPosition] != ';' && ReceivedData[*ParameterEndPosition] != '\n') {
		(*ParameterEndPosition)++;
	}
	if (ReceivedData[*ParameterEndPosition] == '\n') {
		*ReachedEndOfData = true;
	}
}