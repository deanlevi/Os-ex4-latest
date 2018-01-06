/*
Author - Dean Levi 302326640
Project - Ex4
Using - client.h, send_receive.h, user_interface.h
Description - implementation of client operation - initiating client, connecting to server, creating and starting
			  send/receive/user interface threads, and closing all used resources when program finishes.
*/
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#include "client.h"
#include "send_receive.h"
#include "user_interface.h"

/*
Parameters - argv: contains the log file path, port to connect to and the servers ip.
Returns - none.
Description - initialize the database of the client.
*/
void InitClient(char *argv[]);

/*
Parameters - none.
Returns - none.
Description - connects to server and runs the client's threads.
*/
void HandleClient();

/*
Parameters - none.
Returns - none.
Description - sets the client socket and connects to server.
*/
void ConnectToServer();

/*
Parameters - none.
Returns - none.
Description - creates the thread handles and semaphores used by the client.
*/
void CreateThreadsAndSemaphores();

/*
Parameters - none.
Returns - none.
Description - closes all used resources when client program finishes / an error occurs.
*/
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
	Client.GotExitFromUserOrGameFinished = false;
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

	wait_code = WaitForMultipleObjects(NUMBER_OF_THREADS_TO_HANDLE_CLIENT, Client.ThreadHandles, TRUE, INFINITE);
	if (WAIT_OBJECT_0 != wait_code) {
		WriteToLogFile(Client.LogFilePtr, "Custom message: Error when waiting for program to end.\n");
		CloseSocketAndThreads();
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
		sprintf(ConnectMessage, "Failed connecting to server on %s:%d. Exiting.\n", Client.ServerIP, Client.ServerPortNum);
		OutputMessageToWindowAndLogFile(Client.LogFilePtr, ConnectMessage);
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
	sprintf(ConnectMessage, "Connected to server on %s:%d\n", Client.ServerIP, Client.ServerPortNum);
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
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}

	Client.UserInterfaceSemaphore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - not signaled */
		1,		/* Maximum Count */
		NULL);	/* un-named */

	if (Client.UserInterfaceSemaphore == NULL) {
		WriteToLogFile(Client.LogFilePtr, "Custom message: CreateThreadsAndSemaphores - Error when creating UserInterface semaphore.\n");
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
	Client.SendToServerSemaphore = CreateSemaphore(
		NULL,	/* Default security attributes */
		0,		/* Initial Count - not signaled */
		1,		/* Maximum Count */
		NULL);	/* un-named */

	if (Client.SendToServerSemaphore == NULL) {
		WriteToLogFile(Client.LogFilePtr, "Custom message: CreateThreadsAndSemaphores - Error when creating SendToServer semaphore.\n");
		CloseSocketAndThreads();
		exit(ERROR_CODE);
	}
}

void CloseSocketAndThreads() {
	CloseOneSocket(Client.Socket, Client.LogFilePtr);
	int ThreadIndex = 0;
	for (; ThreadIndex < NUMBER_OF_THREADS_TO_HANDLE_CLIENT; ThreadIndex++) {
		CloseOneThreadHandle(Client.ThreadHandles[ThreadIndex], Client.LogFilePtr);
	}
	CloseOneThreadHandle(Client.UserInterfaceSemaphore, Client.LogFilePtr);
	CloseOneThreadHandle(Client.SendToServerSemaphore, Client.LogFilePtr);
	CloseWsaData(Client.LogFilePtr);
}