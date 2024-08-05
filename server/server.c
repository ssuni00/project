#include "server.h"

// 게임 정보 -> clnt 전송
void send_game_info(int clnt_sd, GameInfo *game)
{
    // 게임 정보를 clnt에게 전송
    write(clnt_sd, &game->play_time, sizeof(game->play_time));
    write(clnt_sd, &game->width, sizeof(game->width));
    write(clnt_sd, &game->height, sizeof(game->height));
    write(clnt_sd, &game->player_num, sizeof(game->player_num));
    for (int i = 0; i < game->height; i++)
    {
        write(clnt_sd, game->board[i], game->width);
    }
    write(clnt_sd, game->players, game->player_num * sizeof(Player));
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// game set 초기화하는 함수
void initialize_game(GameInfo *game, int width, int height, int tile_num, int play_time, int player_num)
{
    game->width = width;
    game->height = height;
    game->tile_num = tile_num;
    game->play_time = play_time;
    game->players_ready = 0;
    game->player_num = player_num;
    game->board = malloc(height * sizeof(char *));
    for (int i = 0; i < height; i++)
    {
        game->board[i] = malloc(width);
        memset(game->board[i], ' ', width); // 보드 초기화
    }

    // 타일 초기화 (반반으로 나누어 배치)
    int red_tiles = tile_num / 2;
    int blue_tiles = tile_num - red_tiles; // 타일 수 조정
    for (int i = 0; i < red_tiles + blue_tiles; i++)
    {
        int x, y;
        do
        {
            x = rand() % width;
            y = rand() % height;
        } while (game->board[y][x] != ' '); // 비어있는 위치 찾기

        game->board[y][x] = (i < red_tiles) ? RED : BLUE; // 타일 배치
    }

    // 플레이어 초기화
    game->players = malloc(player_num * sizeof(Player));
    for (int i = 0; i < player_num; i++)
    {
        int x, y;
        do
        {
            x = rand() % width;
            y = rand() % height;
        } while (game->board[y][x] != ' '); // 비어있는 위치 찾기

        game->players[i].player_id = i;
        game->players[i].x = x;
        game->players[i].y = y;
        game->players[i].team = (i % 2 == 0) ? RED : BLUE; // 플레이어 팀 설정
        game->players[i].clnt_sd = -1;
        game->players[i].ready = 0;
    }

    pthread_mutex_init(&game->lock, NULL);
    pthread_cond_init(&game->start_cond, NULL);
}

// 플레이어 명령 처리 함수
void process_player_command(char command, int player_id, GameInfo *game)
{
    Player *player = &game->players[player_id];
    int x = player->x;
    int y = player->y;

    switch (command)
    {
    case 'u': // 위로 이동
        if (y > 0)
            y--;
        break;
    case 'd': // 아래로 이동
        if (y < game->height - 1)
            y++;
        break;
    case 'l': // 왼쪽으로 이동
        if (x > 0)
            x--;
        break;
    case 'r': // 오른쪽으로 이동
        if (x < game->width - 1)
            x++;
        break;
    case ' ': // 엔터로 타일 뒤집기
        if (game->board[player->y][player->x] == RED)
        {
            game->board[player->y][player->x] = BLUE;
        }
        else if (game->board[player->y][player->x] == BLUE)
        {
            game->board[player->y][player->x] = RED;
        }
        return;
    }

    // 위치 이동 처리
    if (game->board[y][x] == ' ')
    {
        player->x = x;
        player->y = y;
    }
}

// 게임 정보 -> all clnt
void send_game_info_to_all_clients(GameInfo *game)
{
    for (int i = 0; i < game->player_num; i++)
    {
        if (game->players[i].ready && game->players[i].clnt_sd != -1)
        {
            send_game_info(game->players[i].clnt_sd, game);
        }
    }
}

// clnt_sd로 게임 정보를 전송하는 함수
void *client_handler(void *arg)
{
    ThreadArg *targ = (ThreadArg *)arg;
    int clnt_sd = targ->clnt_sd;
    GameInfo *game = targ->game;
    char buffer[256];
    ssize_t numBytes;

    game->players[targ->p_num].clnt_sd = clnt_sd; // 클라이언트 소켓 저장

    // 플레이어 ID 전송
    if (write(clnt_sd, &targ->p_num, sizeof(targ->p_num)) <= 0)
    {
        printf("Failed to send player ID to client %d.\n", targ->p_num);
        close(clnt_sd);
        return NULL;
    }

    // 접속 확인을 위해 'y' 입력 받기
    while (1)
    {
        numBytes = read(clnt_sd, buffer, sizeof(buffer)); // 클라이언트로부터 데이터 읽기
        if (numBytes <= 0)
        {
            printf("Client %d disconnected.\n", targ->p_num);
            pthread_mutex_lock(&game->lock);
            game->players[targ->p_num].ready = 0; // 클라이언트가 준비되지 않음으로 설정
            game->players[targ->p_num].clnt_sd = -1;
            pthread_mutex_unlock(&game->lock);
            close(clnt_sd);
            return NULL;
        }
        if (buffer[0] == 'y')
        { // 준비 완료 명령 처리
            pthread_mutex_lock(&game->lock);
            game->players[targ->p_num].ready = 1;
            printf("Client %d is ready.\n", targ->p_num);
            game->players_ready++;
            if (game->players_ready == game->player_num)
            {
                pthread_cond_broadcast(&game->start_cond); // 모든 클라이언트에게 게임 시작 알림
            }
            pthread_mutex_unlock(&game->lock);
            break;
        }
    }

    pthread_mutex_lock(&game->lock);
    while (game->players_ready < game->player_num)
    {
        pthread_cond_wait(&game->start_cond, &game->lock); // 게임 시작 대기
    }
    pthread_mutex_unlock(&game->lock);

    send_game_info(clnt_sd, game); // 초기 게임 상태 전송

    while (game->play_time > 0)
    {                                                     // 게임 시간이 남아있는 동안
        numBytes = read(clnt_sd, buffer, sizeof(buffer)); // 클라이언트로부터 명령 읽기
        if (numBytes <= 0)
        {
            printf("Client %d disconnected during game.\n", targ->p_num);
            pthread_mutex_lock(&game->lock);
            game->players[targ->p_num].ready = 0;
            game->players[targ->p_num].clnt_sd = -1;
            pthread_mutex_unlock(&game->lock);
            close(clnt_sd);
            return NULL;
        }

        printf("Client %d command: %c\n", targ->p_num, buffer[0]); // 명령 출력

        pthread_mutex_lock(&game->lock);
        process_player_command(buffer[0], targ->p_num, game); // 명령 처리
        send_game_info_to_all_clients(game);                  // 모든 클라이언트에 게임 정보 전송
        pthread_mutex_unlock(&game->lock);
    }

    close(clnt_sd);
    return NULL;
}

void update_game_state(GameInfo *game)
{
    pthread_mutex_lock(&game->lock);
    game->play_time--;                   // 게임 시간 감소
    send_game_info_to_all_clients(game); // 모든 클라이언트에 게임 상태 전송
    pthread_mutex_unlock(&game->lock);
}

int main(int argc, char *argv[])
{
    if (argc != 11)
    {
        fprintf(stderr, "Usage: %s -n <player_num> -s <size> -b <tile_num> -t <time> -p <port>\n", argv[0]);
        return 1;
    }

    int serv_sd, clnt_sd, port, player_num, width, height, tile_num, play_time;
    struct sockaddr_in serv_adr, client_addr;
    socklen_t client_addr_size;
    pthread_t *threads;
    GameInfo game;

    // 명령행 인자 parsing
    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-n") == 0)
            player_num = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-s") == 0)
            width = height = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-b") == 0)
            tile_num = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-t") == 0)
            play_time = atoi(argv[i + 1]);
        else if (strcmp(argv[i], "-p") == 0)
            port = atoi(argv[i + 1]);
    }

    printf("Game setup:\nPlayers: %d\nBoard Size: %dx%d\nTiles: %d\nTime: %d seconds\nPort: %d\n\n",
           player_num, width, height, tile_num, play_time, port);

    serv_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sd < 0)
        error_handling("Socket creation failed");

    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(port);

    // 소켓에 주소 할당
    if (bind(serv_sd, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) < 0)
        error_handling("bind() error");

    if (listen(serv_sd, player_num) < 0)
        error_handling("listen() error");

    // 게임 초기화
    initialize_game(&game, width, height, tile_num, play_time, player_num);
    game.play_time = play_time;

    threads = malloc(player_num * sizeof(pthread_t)); // 스레드 배열 메모리 할당
    ThreadArg *thread_args = malloc(player_num * sizeof(ThreadArg));
    client_addr_size = sizeof(client_addr);

    // 각 클라이언트에 대해 스레드 생성
    for (int i = 0; i < player_num; i++)
    {
        clnt_sd = accept(serv_sd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (clnt_sd < 0)
            error_handling("accept() error");

        printf("Player %d has connected.\n", i);
        thread_args[i].p_num = i;
        thread_args[i].clnt_sd = clnt_sd;
        thread_args[i].game = &game;
        pthread_create(&threads[i], NULL, client_handler, &thread_args[i]);
    }

    // 게임 시간이 끝날 때까지 상태 업데이트
    while (game.play_time > 0)
    {
        update_game_state(&game);
        sleep(1); // 1초 대기
    }

    for (int i = 0; i < player_num; i++)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(thread_args);
    close(serv_sd);
    return 0;
}
