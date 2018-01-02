#ifndef SEND_RECEIVE_H
#define SEND_RECEIVE_H

#include "client.h"

#define GAME_STARTED_SIZE 12 // todo check if move to general defines
#define TURN_SWITCH_SIZE 12
#define BOARD_VIEW_SIZE 11
#define BOARD_VIEW_REPLY_LINE_OFFSET 17
#define PLAY_ACCEPTED_SIZE 13
#define PLAY_DECLINED_SIZE 14
#define USER_LIST_REPLY_SIZE 16
#define GAME_STATE_REPLY_SIZE 17
#define NEW_USER_ACCEPTED_OR_DECLINED_SIZE 17
#define GAME_ENDED_SIZE 11

void WINAPI SendThread();
void WINAPI ReceiveThread();

#endif