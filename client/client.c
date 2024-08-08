#include "client.h"

GameInfo game;
pthread_mutex_t game_lock = PTHREAD_MUTEX_INITIALIZER;
int player_id;

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// 현재 게임 보드와 시간을 화면에 출력하는 함수
void print_board(GameInfo *game, int player_id)
{
    // 화면을 지움
    clear();
    printw("Current Game Board (Your team: %c):\n", game->players[player_id].team);
    printw("Remaining Time: %d seconds\n", game->play_time); // 남은 시간 출력

    // 보드의 각 행을 돌면서 출력
    for (int i = 0; i < game->height; i++)
    {
        for (int j = 0; j < game->width; j++)
        {
            int player_found = 0;

            // 모든 플레이어 위치를 확인
            for (int k = 0; k < game->player_num; k++)
            {
                if (game->players[k].y == i && game->players[k].x == j)
                {
                    player_found = 1;
                    // 플레이어의 위치는 팀 색상의 'O'로 표시
                    attron(COLOR_PAIR(game->players[k].team == RED ? 1 : 2));
                    printw("[O]");
                    attroff(COLOR_PAIR(game->players[k].team == RED ? 1 : 2));
                    break;
                }
            }

            // 해당 위치에 플레이어가 없을 경우 보드 타일 출력
            if (!player_found)
            {
                if (game->board[i][j] == RED)
                {
                    attron(COLOR_PAIR(1)); // 빨간색 타일 설정
                    printw("[R]");
                    attroff(COLOR_PAIR(1));
                }
                else if (game->board[i][j] == BLUE)
                {
                    attron(COLOR_PAIR(2)); // 파란색 타일 설정
                    printw("[B]");
                    attroff(COLOR_PAIR(2));
                }
                else
                {
                    printw("[ ]"); // 빈 타일 출력
                }
            }
        }
        printw("\n");
    }
    refresh(); // 화면 갱신
}

// 서버로부터 게임 정보를 수신하고 GameInfo 구조체에 저장하는 함수
int receive_game_info(int sock, GameInfo *game)
{
    pthread_mutex_lock(&game_lock);

    if (read(sock, &game->play_time, sizeof(game->play_time)) <= 0)
    {
        pthread_mutex_unlock(&game_lock);
        return -1; // 오류 발생
    }

    if (read(sock, &game->width, sizeof(game->width)) <= 0)
    {
        pthread_mutex_unlock(&game_lock);
        return -1;
    }

    if (read(sock, &game->height, sizeof(game->height)) <= 0)
    {
        pthread_mutex_unlock(&game_lock);
        return -1;
    }

    if (read(sock, &game->player_num, sizeof(game->player_num)) <= 0)
    {
        pthread_mutex_unlock(&game_lock);
        return -1;
    }

    // 기존 메모리 해제
    if (game->board)
    {
        for (int i = 0; i < game->height; i++)
        {
            free(game->board[i]);
        }
        free(game->board);
    }
    if (game->players)
    {
        free(game->players);
    }

    // 보드 메모리 할당 및 내용 읽기
    game->board = malloc(game->height * sizeof(char *));
    for (int i = 0; i < game->height; i++)
    {
        game->board[i] = malloc(game->width);
        if (read(sock, game->board[i], game->width) <= 0)
        {
            pthread_mutex_unlock(&game_lock);
            return -1;
        }
    }

    // 플레이어 배열 메모리 할당 및 내용 읽기
    game->players = malloc(game->player_num * sizeof(Player));
    if (read(sock, game->players, game->player_num * sizeof(Player)) <= 0)
    {
        pthread_mutex_unlock(&game_lock);
        return -1;
    }

    pthread_mutex_unlock(&game_lock);
    return 0; // 성공
}

// 타일 카운트 결과를 서버로부터 수신하여 출력하는 함수
void receive_tile_counts(int sock)
{
    int red_count, blue_count;
    char winner;

    if (read(sock, &red_count, sizeof(red_count)) <= 0 ||
        read(sock, &blue_count, sizeof(blue_count)) <= 0 ||
        read(sock, &winner, sizeof(winner)) <= 0)
    {
        error_handling("Error receiving tile counts");
    }

    // ncurses 종료
    endwin();

    // 결과 출력
    printf("Game Over!\n");
    printf("Red tiles: %d\n", red_count);
    printf("Blue tiles: %d\n", blue_count);

    if (winner == 'R')
    {
        printf("Red team wins!\n");
    }
    else if (winner == 'B')
    {
        printf("Blue team wins!\n");
    }
    else
    {
        printf("It's a tie!\n");
    }

    // 서버로 종료 확인 메시지 전송
    char quit = 'q';
    if (write(sock, &quit, sizeof(quit)) < 0)
    {
        error_handling("Error sending quit message");
    }
}

// 게임 정보 업데이트 스레드 함수
void *update_game_info_thread(void *arg)
{
    int sock = *(int *)arg;

    while (game.play_time > 0)
    {
        // 서버로부터 업데이트된 게임 정보를 수신
        if (receive_game_info(sock, &game) < 0)
            error_handling("Error receiving updated game info");

        // 게임 보드 출력
        print_board(&game, player_id); // 전역 변수 player_id 사용
        usleep(100000);                // 100ms 동안 대기
    }

    return NULL;
}

// 게임 루프를 실행 -> 플레이어의 입력 처리 & 서버와 통신
void *game_loop(void *arg)
{
    int sock = *(int *)arg;

    char command;
    while (game.play_time > 0)
    {
        int ch = getch(); // 키 입력 받기
        switch (ch)
        {
        case KEY_UP:
            command = 'u'; // 위로 이동 명령
            break;
        case KEY_DOWN:
            command = 'd'; // 아래로 이동 명령
            break;
        case KEY_LEFT:
            command = 'l'; // 왼쪽으로 이동 명령
            break;
        case KEY_RIGHT:
            command = 'r'; // 오른쪽으로 이동 명령
            break;
        case '\n':
            command = ' '; // 타일 뒤집기 명령
            break;
        default:
            continue; // 유효하지 않은 키 입력은 무시
        }

        // 명령을 서버로 전송
        if (write(sock, &command, sizeof(command)) < 0)
            error_handling("Error sending command");
    }

    // 게임 시간이 0초가 되었을 때 타일 카운트 결과 수신 및 출력
    receive_tile_counts(sock);

    return NULL;
}

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
    struct sockaddr_in serv_adr;

    // 소켓 생성
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        error_handling("socket() error");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = inet_addr(ip);
    serv_adr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("connect() error");

    printf("Press 'y' to confirm connection: ");
    char ch;
    while ((ch = getchar()) != 'y')
        ;

    // 연결 확인 메시지를 서버로 전송
    if (write(sock, &ch, sizeof(ch)) < 0)
        error_handling("Error sending confirmation");

    // 플레이어 ID 수신
    if (read(sock, &player_id, sizeof(player_id)) <= 0)
        error_handling("Error receiving player ID");

    // 서버로부터 초기 게임 정보를 수신
    if (receive_game_info(sock, &game) < 0)
        error_handling("Error receiving game info");

    // ncurses 초기화 및 설정
    initscr();
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);  // 빨간색 설정
    init_pair(2, COLOR_BLUE, COLOR_BLACK); // 파란색 설정
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    pthread_t update_thread, input_thread;

    // 게임 정보 업데이트를 위한 스레드 생성
    if (pthread_create(&update_thread, NULL, update_game_info_thread, &sock) != 0)
    {
        error_handling("Error creating update thread");
    }

    // 게임 루프를 위한 스레드 생성
    if (pthread_create(&input_thread, NULL, game_loop, &sock) != 0)
    {
        error_handling("Error creating input thread");
    }

    // 스레드 종료 대기
    pthread_join(update_thread, NULL);
    pthread_join(input_thread, NULL);

    // ncurses 종료 및 정리
    endwin();

    // 메모리 해제
    for (int i = 0; i < game.height; i++)
    {
        free(game.board[i]);
    }
    free(game.board);
    free(game.players);

    close(sock);
    return 0;
}
