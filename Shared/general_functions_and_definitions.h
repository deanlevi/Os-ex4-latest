#ifndef GENERAL_FUNCTIONS_AND_DEFINITIONS_H
#define GENERAL_FUNCTIONS_AND_DEFINITIONS_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // todo move to shared
#endif

#include <WinSock2.h> // todo move to shared
#include <WS2tcpip.h> // todo move to shared

#pragma comment(lib, "Ws2_32.lib") // todo move to shared

#define SUCCESS_CODE (int)(0) // todo move to shared
#define ERROR_CODE (int)(-1) // todo move to shared
#define MESSAGE_LENGTH 100
#define USER_NAME_LENGTH 31
#define BOARD_SIZE 3
#define BOARD_MESSAGE_LINE_OFFSET 11
#define BOARD_MESSAGE_LINE_LENGTH 18

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

#endif