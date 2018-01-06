/*
Author - Dean Levi 302326640
Project - Ex4
Using - client.h
Description - main file that initiates the client and its parameters, calls to handle client that starts the client operation and after
			  the client finished closes all used sockets and thread handles.
*/
#include <stdio.h>

#include "client.h"

int main(int argc, char *argv[]) {
	if (argc != 5) {
		printf("Not the right amount of input arguments.\nNeed to give four.\nExiting...\n"); // first is path, other four are inputs
		return ERROR_CODE;
	}
	InitClient(argv);
	HandleClient();
	CloseSocketAndThreads();

	return SUCCESS_CODE;
}