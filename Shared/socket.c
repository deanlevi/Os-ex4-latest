#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "socket.h"

SOCKET CreateOneSocket();
int SendData(SOCKET Socket, char *DataToSend, char *LogFilePathPtr);
char *ReceiveData(SOCKET Socket, char *LogFilePathPtr);
void CloseOneSocket(SOCKET Socket, char *LogFilePathPtr);


SOCKET CreateOneSocket() {
	SOCKET NewSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	return NewSocket;
}

int SendData(SOCKET Socket, char *DataToSend, char *LogFilePathPtr) {
	size_t BytesToSend = strlen(DataToSend) + 1; // + 1 for '\0'
	char *CurrentPositionInDataToSend = DataToSend;
	int SentBytes;
	size_t RemainingBytesToSend = BytesToSend;

	while (RemainingBytesToSend > 0) {
		SentBytes = send(Socket, CurrentPositionInDataToSend, RemainingBytesToSend, SEND_RECEIVE_FLAGS);
		if (SentBytes == SOCKET_ERROR) {
			OutputMessageToWindowAndLogFile(LogFilePathPtr, "Custom message: SendData failed to send. Exiting...\n");
			return ERROR_CODE;
		}
		RemainingBytesToSend -= SentBytes;
		CurrentPositionInDataToSend += SentBytes;
	}
	return SUCCESS_CODE;
}

char *ReceiveData(SOCKET Socket, char *LogFilePathPtr) {
	int ReceivedBytes = 0;
	char CurrentReceivedBuffer[MESSAGE_LENGTH];
	int SizeOfWholeReceivedBuffer = MESSAGE_LENGTH;
	int IndexInWholeReceivedBuffers = 0;
	bool NeedToReallocWholeBuffer = false;
	
	char *WholeReceivedBuffer = malloc(sizeof(char) * (MESSAGE_LENGTH + 1)); // todo need to free
	if (WholeReceivedBuffer == NULL) {
		OutputMessageToWindowAndLogFile(LogFilePathPtr, "Custom message: ReceiveData failed to allocate memory.\n");
		return NULL;
	}
	WholeReceivedBuffer[IndexInWholeReceivedBuffers] = '\0'; // so that strstr will work // todo

	while (strstr(WholeReceivedBuffer, "\n") == NULL) {
		ReceivedBytes = recv(Socket, CurrentReceivedBuffer, MESSAGE_LENGTH, SEND_RECEIVE_FLAGS);
		if (ReceivedBytes == SOCKET_ERROR) {
			OutputMessageToWindowAndLogFile(LogFilePathPtr, "Custom message: ReceiveAndSendData failed to recv. Exiting...\n");
			return NULL;
		}

		NeedToReallocWholeBuffer = IndexInWholeReceivedBuffers + ReceivedBytes > SizeOfWholeReceivedBuffer;
		if (NeedToReallocWholeBuffer) {
			SizeOfWholeReceivedBuffer += MESSAGE_LENGTH;
			WholeReceivedBuffer = (char*)realloc(WholeReceivedBuffer, SizeOfWholeReceivedBuffer);
			if (WholeReceivedBuffer == NULL) {
				OutputMessageToWindowAndLogFile(LogFilePathPtr, "Custom message: ReceiveData failed to allocate memory.\n");
				return NULL;
			}
		}
		strncat(WholeReceivedBuffer, CurrentReceivedBuffer, ReceivedBytes);
		IndexInWholeReceivedBuffers += ReceivedBytes;
	}
	return WholeReceivedBuffer;
}

void CloseOneSocket(SOCKET Socket, char *LogFilePathPtr) {
	int CloseSocketReturnValue;
	if (Socket != INVALID_SOCKET) {
		CloseSocketReturnValue = closesocket(Socket);
		if (CloseSocketReturnValue == SOCKET_ERROR) {
			char ErrorMessage[MESSAGE_LENGTH];
			sprintf(ErrorMessage, "Custom message: CloseOneSocket failed to close socket. Error Number is %d\n", WSAGetLastError());
			OutputMessageToWindowAndLogFile(LogFilePathPtr, ErrorMessage);
			exit(ERROR_CODE);
		}
	}
}