#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ncurses.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define RED 'R'
#define BLUE 'B'
#define LEFT 75
#define RIGHT 77
#define UP 72
#define DOWN 80

typedef struct
{
    int player_id;
    int team;
    int x;
    int y;
    int ready;
    int client_sock;
} Player;

typedef struct
{
    char **board;
    Player *players;
    int play_time;
    int width;
    int height;
    int tile_num;
    int red_tiles;
    int blue_tiles;
    int players_ready;
    int player_num;
    pthread_mutex_t lock; // 뮤텍스 추가
} GameInfo;

void error_handling(char *message);
void connect_to_server(const char *ip, int port, int *sock);
void *game_loop(void *arg); // 스레드 함수 선언
void print_board(GameInfo *game, int player_id);
void send_command(int sock, char command);
int receive_game_info(int sock, GameInfo *game);
void *update_game_info_thread(void *arg); // 스레드 함수 선언

#endif
