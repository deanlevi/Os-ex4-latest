/*
Author - Dean Levi 302326640
Project - Ex4
Using - server.h
Description - declaration of the thread function that runs the game and needed defines.
*/
#ifndef TIC_TAC_TOE_H
#define TIC_TAC_TOE_H

#include "server.h"

#define BOARD_VIEW_QUERY_SIZE 16

#define PLAY_REQUEST_SIZE 13
#define USER_LIST_QUERY_SIZE 15
#define GAME_STATE_QUERY_SIZE 16

void WINAPI TicTacToeGameThread(LPVOID lpParam);

#endif