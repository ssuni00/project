#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define RED 'R'
#define BLUE 'B'
#define EMPTY ' '
#define LEFT 75
#define RIGHT 77
#define UP 72
#define DOWN 80

typedef struct
{
    int player_id; // 플레이어 ID
    char team;     // 팀 (RED 또는 BLUE)
    int x;         // x 좌표
    int y;         // y 좌표
    int ready;     // 준비 상태 (0 또는 1)
    int clnt_sd;   // 클라이언트 소켓 디스크립터
} Player;

typedef struct
{
    int width;                 // 보드의 너비
    int height;                // 보드의 높이
    int tile_num;              // 타일의 수
    int play_time;             // 남은 게임 시간
    int player_num;            // 플레이어의 수
    int players_ready;         // 준비 완료된 플레이어 수
    char **board;              // 보드의 상태
    Player *players;           // 플레이어 배열
    pthread_mutex_t lock;      // 뮤텍스
    pthread_cond_t start_cond; // 조건 변수
} GameInfo;

typedef struct
{
    int p_num;
    int clnt_sd;
    GameInfo *game;
} ThreadArg;

void error_handling(char *message);
void send_game_info(int clnt_sd, GameInfo *game);
void initialize_game(GameInfo *game, int width, int height, int tile_num, int play_time, int player_num);
void process_player_command(char command, int player_id, GameInfo *game);
void send_game_info_to_all_clients(GameInfo *game);
void *client_handler(void *arg);
void update_game_state(GameInfo *game);

#endif // SERVER_H
