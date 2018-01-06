/*
Author - Dean Levi 302326640
Project - Ex4
Using - server.h
Description - declaration of the functions that are used to connect clients to the server and needed define.
*/
#ifndef CONNECT_CLIENTS_H
#define CONNECT_CLIENTS_H

#include "server.h"

#define MINUTE_IN_MS 60000

void HandleConnectToClients();
void WINAPI ConnectToClientsThread(LPVOID lpParam);

#endif