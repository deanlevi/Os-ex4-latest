#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#include "connect_clients.h"
#include "tic_tac_toe.h"

void HandleConnectToClients();
void WINAPI ConnectToClientsThread(LPVOID lpParam);

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
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		TimeToWait = ClientIndex == 0 ? MINUTE_IN_MS : (4 * MINUTE_IN_MS); // wait one minute for first client and four more for second
		wait_code = WaitForSingleObject(Server.ConnectUsersThreadHandle, TimeToWait); // wait for client
		if (WAIT_OBJECT_0 != wait_code) {
			Server.WaitingForClientsTimedOut = true;
			OutputMessageToWindowAndLogFile(Server.LogFilePtr, "No players connected. Exiting.\n");
			CloseOneThreadHandle(Server.ConnectUsersThreadHandle, Server.LogFilePtr); // close thread handle
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		CloseOneThreadHandle(Server.ConnectUsersThreadHandle, Server.LogFilePtr); // close thread handle
	}
}

void WINAPI ConnectToClientsThread(LPVOID lpParam) {
	int *ClientIndex = (int*)lpParam;
	while (Server.NumberOfConnectedUsers <= *ClientIndex) { // while clients are declined
		Server.ClientsSockets[*ClientIndex] = accept(Server.ListeningSocket, NULL, NULL);
		if (Server.WaitingForClientsTimedOut) {
			break;
		}
		if (Server.ClientsSockets[*ClientIndex] == INVALID_SOCKET) {
			char ErrorMessage[MESSAGE_LENGTH];
			sprintf(ErrorMessage, "Custom message: ConnectToClients failed to accept. Error Number is %d\n", WSAGetLastError());
			WriteToLogFile(Server.LogFilePtr, ErrorMessage);
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		Server.ClientsThreadHandle[*ClientIndex] = CreateThreadSimple((LPTHREAD_START_ROUTINE)TicTacToeGameThread,
																	  &Server.Players[*ClientIndex].ClientIndex,
																	  &Server.ClientsThreadID[*ClientIndex],
																	   Server.LogFilePtr);
		if (Server.ClientsThreadHandle[*ClientIndex] == NULL) {
			CloseSocketsAndThreads();
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