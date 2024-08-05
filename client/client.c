#include "client.h"

// 오류 처리를 위한 함수
void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// 보드 출력 함수
void print_board(GameInfo *game, int player_id)
{
    clear();
    printw("Current Game Board for Player %d:\n", player_id);
    for (int i = 0; i < game->height; i++)
    {
        for (int j = 0; j < game->width; j++)
        {
            int player_found = 0;
            for (int k = 0; k < game->player_num; k++)
            {
                if (game->players[k].y == i && game->players[k].x == j)
                {
                    player_found = 1;
                    attron(COLOR_PAIR(game->players[k].team == RED ? 1 : 2));
                    printw("[O]");
                    attroff(COLOR_PAIR(game->players[k].team == RED ? 1 : 2));
                    break;
                }
            }
            if (!player_found)
            {
                if (game->board[i][j] == RED)
                {
                    attron(COLOR_PAIR(1));
                    printw("[R]");
                    attroff(COLOR_PAIR(1));
                }
                else if (game->board[i][j] == BLUE)
                {
                    attron(COLOR_PAIR(2));
                    printw("[B]");
                    attroff(COLOR_PAIR(2));
                }
                else
                {
                    printw("[ ]");
                }
            }
        }
        printw("\n");
    }
    refresh();
}

// 서버로부터 게임 정보를 받는 함수
int receive_game_info(int sock, GameInfo *game)
{
    if (read(sock, &game->play_time, sizeof(game->play_time)) <= 0)
        return -1; // 오류 발생

    if (read(sock, &game->width, sizeof(game->width)) <= 0)
        return -1; // 오류 발생

    if (read(sock, &game->height, sizeof(game->height)) <= 0)
        return -1; // 오류 발생

    if (read(sock, &game->player_num, sizeof(game->player_num)) <= 0)
        return -1; // 오류 발생

    game->board = malloc(game->height * sizeof(char *));
    for (int i = 0; i < game->height; i++)
    {
        game->board[i] = malloc(game->width);
        if (read(sock, game->board[i], game->width) <= 0)
            return -1; // 오류 발생
    }

    game->players = malloc(game->player_num * sizeof(Player));
    if (read(sock, game->players, game->player_num * sizeof(Player)) <= 0)
        return -1; // 오류 발생

    return 0; // 성공
}

// 게임 루프 함수
void game_loop(int sock)
{
    GameInfo game;
    int player_id;

    // 플레이어 ID 수신
    if (read(sock, &player_id, sizeof(player_id)) <= 0)
    {
        error_handling("Error receiving player ID");
    }

    if (receive_game_info(sock, &game) < 0)
    {
        error_handling("Error receiving game info");
    }

    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_BLUE, COLOR_BLACK);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    char command;
    while (1)
    {
        print_board(&game, player_id);

        int ch = getch();
        switch (ch)
        {
        case KEY_UP:
            command = 'u';
            break;
        case KEY_DOWN:
            command = 'd';
            break;
        case KEY_LEFT:
            command = 'l';
            break;
        case KEY_RIGHT:
            command = 'r';
            break;
        case '\n':
            command = ' ';
            break;
        default:
            continue;
        }

        if (write(sock, &command, sizeof(command)) < 0)
            error_handling("Error sending command");

        if (receive_game_info(sock, &game) < 0)
        {
            error_handling("Error receiving updated game info");
        }
    }

    endwin();

    // 메모리 해제
    for (int i = 0; i < game.height; i++)
    {
        free(game.board[i]);
    }
    free(game.board);
    free(game.players);
}

// 메인 함수
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        error_handling("Socket creation failed");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        error_handling("Connection failed");

    printf("Press 'y' to confirm connection: ");
    char ch;
    while ((ch = getchar()) != 'y')
        ;

    if (write(sock, &ch, sizeof(ch)) < 0)
        error_handling("Error sending confirmation");

    game_loop(sock); // 게임 루프 실행

    close(sock); // 소켓 닫기
    return 0;
}
