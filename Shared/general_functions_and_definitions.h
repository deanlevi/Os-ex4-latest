/*
Author - Dean Levi 302326640
Project - Ex4
Using - none
Description - declaration of general functions and defines used by both server and client.
*/
#ifndef GENERAL_FUNCTIONS_AND_DEFINITIONS_H
#define GENERAL_FUNCTIONS_AND_DEFINITIONS_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#define SUCCESS_CODE (int)(0)
#define ERROR_CODE (int)(-1)
#define SEND_MESSAGES_WAIT 20
#define MESSAGE_LENGTH 100
#define USER_NAME_LENGTH 31
#define BOARD_SIZE 3
#define BOARD_VIEW_LINE_OFFSET 11
#define BOARD_VIEW_LINE_LENGTH 18
#define BOARD_VIEW_QUERY_LINE_LENGTH 24

typedef enum _PlayerType {
	None,
	X,
	O
}PlayerType;

void InitWsaData();
void InitLogFile(char *LogFilePathPtr);
void OutputMessageToWindowAndLogFile(char *LogFilePathPtr, char *MessageToWrite);
void WriteToLogFile(char *LogFilePathPtr, char *MessageToWrite);
HANDLE CreateThreadSimple(LPTHREAD_START_ROUTINE p_start_routine, LPVOID p_thread_parameters, LPDWORD p_thread_id,
						  char *LogFilePathPtr);
BOOL ReleaseOneSemaphore(HANDLE Semaphore);
void CloseOneThreadHandle(HANDLE HandleToClose, char *LogFilePathPtr);
void CloseWsaData(char *LogFilePathPtr);
int FindEndOfDataSymbol(char *ReceivedData);

#endif