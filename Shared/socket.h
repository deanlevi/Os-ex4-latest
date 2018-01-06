/*
Author - Dean Levi 302326640
Project - Ex4
Using - general_functions_and_definitions.h
Description - declaration of general functions of send/receive and closing sockets used by both server and client.
			  also needed defines.
*/
#ifndef SOCKET_H
#define SOCKET_H

#include "general_functions_and_definitions.h"

#define SEND_RECEIVE_FLAGS 0
#define RECV_FINISHED 0

SOCKET CreateOneSocket();
int SendData(SOCKET Socket, char *DataToSend, char *LogFilePathPtr);
char *ReceiveData(SOCKET Socket, char *LogFilePathPtr);
void CloseOneSocket(SOCKET Socket, char *LogFilePathPtr);

#endif