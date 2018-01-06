/*
Author - Dean Levi 302326640
Project - Ex4
Using - none
Description - implementation of general functions and defines used by both server and client.
*/
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#include "general_functions_and_definitions.h"

/*
Parameters - none.
Returns - none.
Description - init wsa data.
*/
void InitWsaData();

/*
Parameters - LogFilePathPtr: log file path in case of an error.
Returns - none.
Description - init log file.
*/
void InitLogFile(char *LogFilePathPtr);

/*
Parameters - LogFilePathPtr: log file path in case of an error, MessageToWrite: message to write to log.
Returns - none.
Description - writes the input message to screen and log file.
*/
void OutputMessageToWindowAndLogFile(char *LogFilePathPtr, char *MessageToWrite);

/*
Parameters - LogFilePathPtr: log file path in case of an error, MessageToWrite: message to write to log.
Returns - none.
Description - writes the input message to log file.
*/
void WriteToLogFile(char *LogFilePathPtr, char *MessageToWrite);

/*
Parameters - p_start_routine - a pointer to the function to be executed by the thread,
			 p_thread_id - a pointer to a variable that receives the thread identifier (output parameter),
			 p_thread_argument - the argument to send to thread's function.
Returns - if the function succeeds, the return value is a handle to the new thread, if not prints error and returns NULL.
Description - creating the thread with the right parameters.
*/
HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine, LPVOID p_thread_parameters, LPDWORD p_thread_id,
						  char *LogFilePathPtr);

/*
Parameters - Semaphore: the semaphore to be released.
Returns - TRUE if succeeded, FALSE if failed.
Description - release one semaphore.
*/
BOOL ReleaseOneSemaphore(HANDLE Semaphore);

/*
Parameters - HandleToClose: the handle to be closed, LogFilePathPtr: log file path in case of an error.
Returns - none.
Description - close one thread handle.
*/
void CloseOneThreadHandle(HANDLE HandleToClose, char *LogFilePathPtr);

/*
Parameters - LogFilePathPtr: log file path in case of an error.
Returns - none.
Description - close wsa data.
*/
void CloseWsaData(char *LogFilePathPtr);

/*
Parameters - ReceivedData: the data that was received from server.
Returns - the end position of the parsed parameter.
Description - searches and updates the end position of the parsed parameter in ReceivedData.
*/
int FindEndOfDataSymbol(char *ReceivedData);

void InitWsaData() {
	WSADATA wsaData;
	int StartupRes = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (StartupRes != NO_ERROR) {
		printf("Custom message: Error %ld at WSAStartup().\nExiting...\n", StartupRes);
		// Tell the user that we could not find a usable WinSock DLL.
		exit(ERROR_CODE);
	}
}

void InitLogFile(char *LogFilePathPtr) {
	FILE *LogFilePointer = fopen(LogFilePathPtr, "w");
	if (LogFilePointer == NULL) {
		printf("Custom message: Couldn't open log file.\n");
		exit(ERROR_CODE);
	}
	fclose(LogFilePointer);
}

void OutputMessageToWindowAndLogFile(char *LogFilePathPtr, char *MessageToWrite) {
	printf("%s", MessageToWrite);
	WriteToLogFile(LogFilePathPtr, MessageToWrite);
}

void WriteToLogFile(char *LogFilePathPtr, char *MessageToWrite) {
	FILE *LogFilePointer = fopen(LogFilePathPtr, "a");
	if (LogFilePointer == NULL) {
		printf("Custom message: Couldn't open log file.\n");
		exit(ERROR_CODE);
	}
	fputs(MessageToWrite, LogFilePointer);
	fclose(LogFilePointer);
}

HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine, LPVOID p_thread_parameters, LPDWORD p_thread_id,
						  char *LogFilePathPtr) {
	HANDLE thread_handle;

	if (NULL == p_start_routine) {
		OutputMessageToWindowAndLogFile(LogFilePathPtr, "Custom message: Error when creating a thread. Received null pointer.\n");
		return NULL;
	}

	if (NULL == p_thread_id) {
		OutputMessageToWindowAndLogFile(LogFilePathPtr, "Custom message: Error when creating a thread. Received null pointer.\n");
		return NULL;
	}

	thread_handle = CreateThread(
		NULL,                /*  default security attributes */
		0,                   /*  use default stack size */
		p_start_routine,     /*  thread function */
		p_thread_parameters, /*  argument to thread function */
		0,                   /*  use default creation flags */
		p_thread_id);        /*  returns the thread identifier */

	if (NULL == thread_handle) {
		OutputMessageToWindowAndLogFile(LogFilePathPtr, "Custom message: Couldn't create thread.\n");
		return NULL;
	}

	return thread_handle;
}

BOOL ReleaseOneSemaphore(HANDLE Semaphore) {
	BOOL release_res;

	release_res = ReleaseSemaphore(
		Semaphore,
		1,
		NULL);
	return release_res;
}

void CloseOneThreadHandle(HANDLE HandleToClose, char *LogFilePathPtr) {
	DWORD ret_val;
	if (HandleToClose != NULL) {
		ret_val = CloseHandle(HandleToClose);
		if (FALSE == ret_val) {
			OutputMessageToWindowAndLogFile(LogFilePathPtr, "Custom message: Error when closing thread handle.\n");
			exit(ERROR_CODE);
		}
	}
}

void CloseWsaData(char *LogFilePathPtr) {
	if (WSACleanup() == SOCKET_ERROR) {
		char ErrorMessage[MESSAGE_LENGTH];
		sprintf(ErrorMessage, "Custom message: Failed to close Winsocket, error %ld. Ending program.\n", WSAGetLastError());
		OutputMessageToWindowAndLogFile(ErrorMessage, LogFilePathPtr);
		exit(ERROR_CODE);
	}
}

int FindEndOfDataSymbol(char *ReceivedData) { // find '\n' index in ReceivedData
	int EndSymbolPosition = 0;
	while (ReceivedData[EndSymbolPosition] != '\n') {
		EndSymbolPosition++;
	}
	return EndSymbolPosition;
}