#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#include "general_functions_and_definitions.h"

void InitWsaData();
void InitLogFile(char *LogFilePathPtr);
void OutputMessageToWindowAndLogFile(char *LogFilePathPtr, char *MessageToWrite);
void WriteToLogFile(char *LogFilePathPtr, char *MessageToWrite);
HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine, LPVOID p_thread_parameters, LPDWORD p_thread_id,
						  char *LogFilePathPtr);
BOOL ReleaseOneSemaphore(HANDLE Semaphore);
void CloseOneThreadHandle(HANDLE HandleToClose, char *LogFilePathPtr);
void CloseWsaData(char *LogFilePathPtr);

void InitWsaData() {
	WSADATA wsaData;
	int StartupRes = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (StartupRes != NO_ERROR) {
		printf("Custom message: Error %ld at WSAStartup().\nExiting...\n", StartupRes);
		// Tell the user that we could not find a usable WinSock DLL.
		exit(ERROR_CODE); // todo handle error
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
	WriteToLogFile(LogFilePathPtr, MessageToWrite); // todo
}

void WriteToLogFile(char *LogFilePathPtr, char *MessageToWrite) { // todo check all calls to this function
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