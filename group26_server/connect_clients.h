#ifndef CONNECT_CLIENTS_H
#define CONNECT_CLIENTS_H

#include "server.h"

#define MINUTE_IN_MS 60000

void HandleConnectToClients();
void WINAPI ConnectToClientsThread(LPVOID lpParam);

#endif