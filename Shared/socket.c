/*
Author - Dean Levi 302326640
Project - Ex4
Using - general_functions_and_definitions.h
Description - implementation of general functions of send/receive and closing sockets used by both server and client.
*/
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "socket.h"

/*
Parameters - none.
Returns - a new created socket.
Description - creates one socket and returns it.
*/
SOCKET CreateOneSocket();

/*
Parameters - Socket: the socket that is sending, DataToSend: the data to send, LogFilePathPtr: log file path in case of an error.
Returns - SUCCESS_CODE if sending succeeded, else ERROR_CODE.
Description - sending data through socket.
*/
int SendData(SOCKET Socket, char *DataToSend, char *LogFilePathPtr);

/*
Parameters - Socket: the socket that is receiving, LogFilePathPtr: log file path in case of an error.
Returns - the received data.
Description - receive data through the socket and return it.
*/
char *ReceiveData(SOCKET Socket, char *LogFilePathPtr);

/*
Parameters - Socket: the socket to be closed, LogFilePathPtr: log file path in case of an error.
Returns - none.
Description - closing one socket.
*/
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
	
	char *WholeReceivedBuffer = malloc(sizeof(char) * (MESSAGE_LENGTH + 1));
	if (WholeReceivedBuffer == NULL) {
		OutputMessageToWindowAndLogFile(LogFilePathPtr, "Custom message: ReceiveData failed to allocate memory.\n");
		return NULL;
	}
	WholeReceivedBuffer[IndexInWholeReceivedBuffers] = '\0'; // so that strstr will work

	while (strstr(WholeReceivedBuffer, "\n") == NULL) {
		ReceivedBytes = recv(Socket, CurrentReceivedBuffer, MESSAGE_LENGTH, SEND_RECEIVE_FLAGS);
		if (ReceivedBytes == SOCKET_ERROR) {
			OutputMessageToWindowAndLogFile(LogFilePathPtr, "Custom message: ReceiveAndSendData failed to recv. Exiting...\n");
			return NULL;
		}
		if (ReceivedBytes == RECV_FINISHED) {
			shutdown(Socket, SD_SEND);
			strcpy(WholeReceivedBuffer, "FINISHED");
			return WholeReceivedBuffer;
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