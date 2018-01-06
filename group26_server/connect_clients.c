/*
Author - Dean Levi 302326640
Project - Ex4
Using - connect_clients.h, tic_tac_toe.h
Description - implementation of the functions that are used to connect clients to the server and handling clients requests.
*/
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#include "connect_clients.h"
#include "tic_tac_toe.h"

/*
Parameters - none.
Returns - none.
Description - handle the connection of clients to the server.
*/
void HandleConnectToClients();

/*
Parameters - lpParam: pointer to ClientIndex.
Returns - none.
Description - the thread connecting clients to the server and declining clients after the first two connected.
*/
void WINAPI ConnectToClientsThread(LPVOID lpParam);

void HandleConnectToClients() {
	DWORD wait_code;
	int ClientIndex = 0;
	DWORD TimeToWait;
	for (; ClientIndex < NUMBER_OF_CLIENTS + 1; ClientIndex++) { // connect each client with different waiting time
		Server.ConnectUsersThreadHandle = CreateThreadSimple((LPTHREAD_START_ROUTINE)ConnectToClientsThread,
															 &Server.Players[ClientIndex].ClientIndex,
															 &Server.ConnectUsersThreadID,
															  Server.LogFilePtr);
		if (Server.ConnectUsersThreadHandle == NULL) {
			CloseSocketsAndThreads();
			exit(ERROR_CODE);
		}
		TimeToWait = ClientIndex == 0 ? (5 * MINUTE_IN_MS) : INFINITE; // wait five minutes for first client and then as needed for second
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
	if (Server.NumberOfConnectedUsers < NUMBER_OF_CLIENTS) { // for first two clients
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
				CloseSocketsAndThreads();
				exit(ERROR_CODE);
			}
		}
	}
	else { // decline more clients
		char *DeclineMessage = "NEW_USER_DECLINED\n";
		int SendDataToServerReturnValue;
		while (TRUE) {
			Server.ClientsSockets[NUMBER_OF_CLIENTS] = accept(Server.ListeningSocket, NULL, NULL);
			if (Server.ClientsSockets[NUMBER_OF_CLIENTS] == INVALID_SOCKET) {
				char ErrorMessage[MESSAGE_LENGTH];
				sprintf(ErrorMessage, "Custom message: ConnectToClients failed to accept. Error Number is %d\n", WSAGetLastError());
				WriteToLogFile(Server.LogFilePtr, ErrorMessage);
				CloseSocketsAndThreads();
				exit(ERROR_CODE);
			}
			Sleep(SEND_MESSAGES_WAIT); // to let new client send request before declining
			SendDataToServerReturnValue = SendData(Server.ClientsSockets[NUMBER_OF_CLIENTS], DeclineMessage, Server.LogFilePtr);
			if (SendDataToServerReturnValue == ERROR_CODE) {
				CloseSocketsAndThreads();
				exit(ERROR_CODE);
			}
		}
	}
}