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