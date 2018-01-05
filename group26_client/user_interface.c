#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#include "user_interface.h"

void WINAPI UserInterfaceThread();
void WaitForUserInterfaceSemaphore();
void HandleInputFromUser(char *UserInput);

void WINAPI UserInterfaceThread() {
	WaitForUserInterfaceSemaphore();
	char UserInput[USER_INPUT_LENGTH];
	while (TRUE) {
		if (strcmp(Client.MessageToSendToServer, "FINISHED") == 0) { // if finished
			break; // finished communication
		}
		scanf("%s", UserInput);
		/*if (strcmp(Client.MessageToSendToServer, "FINISHED") == 0) { // if finished
			break; // finished communication
		}*/
		HandleInputFromUser(UserInput);
	}
}

void WaitForUserInterfaceSemaphore() {
	DWORD wait_code;

	wait_code = WaitForSingleObject(Client.UserInterfaceSemaphore, INFINITE); // wait for connection to be established and user accepted
	if (WAIT_OBJECT_0 != wait_code) {
		WriteToLogFile(Client.LogFilePtr, "Custom message: SendThread - failed to wait for UserInterface semaphore.\n");
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
		strcpy(Client.MessageToSendToServer, "FINISHED");
		Client.GotExitFromUser = true;
		shutdown(Client.Socket, SD_BOTH);
	}
	else {
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, "Error: Illegal command\n");
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