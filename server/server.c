#include "server.h"

// 게임 정보 -> clnt 전송
void send_game_info(int clnt_sd, GameInfo *game)
{
    write(clnt_sd, &game->play_time, sizeof(game->play_time));
    write(clnt_sd, &game->width, sizeof(game->width));
    write(clnt_sd, &game->height, sizeof(game->height));
    write(clnt_sd, &game->player_num, sizeof(game->player_num));

    // 보드의 각 행에 대한 정보 전송
    for (int i = 0; i < game->height; i++)
    {
        write(clnt_sd, game->board[i], game->width);
    }

    // 모든 플레이어 정보 -> clnt 전송
    write(clnt_sd, game->players, game->player_num * sizeof(Player));
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// 게임 설정 초기화
void initialize_game(GameInfo *game, int width, int height, int tile_num, int play_time, int player_num)
{
    game->width = width;
    game->height = height;
    game->tile_num = tile_num;
    game->play_time = play_time;
    game->players_ready = 0;
    game->player_num = player_num;
    game->board = malloc(height * sizeof(char *)); // 보드 메모리 할당

    // 보드의 각 행 mem allocate & initialize
    for (int i = 0; i < height; i++)
    {
        game->board[i] = malloc(width);
        memset(game->board[i], ' ', width); // 보드 초기화
    }

    // 타일을 보드에 배치
    int red_tiles = tile_num / 2;
    int blue_tiles = tile_num - red_tiles;
    for (int i = 0; i < red_tiles + blue_tiles; i++)
    {
        int x, y;

        // 보드에서 비어있는 위치(' ')를 찾을 때까지 계속 반복
        do
        {
            // 보드의 너비와 높이 내에서 랜덤한 값을 가짐
            x = rand() % width;
            y = rand() % height;
        } while (game->board[y][x] != ' '); // 해당 위치가 비어있지 않으면 -> 다시 랜덤 위치 찾기

        game->board[y][x] = (i < red_tiles) ? 'R' : 'B'; // 타일 배치
    }

    // 플레이어를 보드에 배치
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
        // index 짝수: RED & 홀수: BLUE
        game->players[i].team = (i % 2 == 0) ? 'R' : 'B';
        // 플레이어의 clnt_sd 초기화 = 아직 클라이언트와 연결되지 않았음을 나타냄
        game->players[i].clnt_sd = -1;
        // 플레이어의 준비 상태를 0으로 초기화 = 준비되지 않은 상태를 나타냄 (y 누르기전)
        game->players[i].ready = 0;
    }

    // 여러 thr가 동시에 game 구조체를 액세스하고 수정할 수 있도록
    // multi thr가 동시에 게임 상태 업데이트 or 플레이어의 준비 상태 변경할 때  -> 데이터 경쟁 방지
    pthread_mutex_init(&game->lock, NULL);
    // 특정 조건이 발생할 때까지 하나 이상의 스레드를 대기시키는 데 사용
    // game->start_cond 조건 변수를 사용 -> 플레이어들이 모두 준비될 때까지 대기 ->
    // pthread_cond_broadcast를 호출 -> 대기 중인 모든 스레드에 신호를 보냄
    pthread_cond_init(&game->start_cond, NULL);
}

// 플레이어 위치 명령 처리
void process_player_command(char command, int player_id, GameInfo *game)
{
    Player *player = &game->players[player_id];
    int newX = player->x;
    int newY = player->y;

    // 명령에 따라 플레이어의 위치를 변경
    switch (command)
    {
    case 'u': // 위로 이동
        newY--;
        break;
    case 'd': // 아래로 이동
        newY++;
        break;
    case 'l': // 왼쪽으로 이동
        newX--;
        break;
    case 'r': // 오른쪽으로 이동
        newX++;
        break;
    case ' ': // 엔터로 타일 뒤집기
        if (game->board[player->y][player->x] == 'R')
        {
            game->board[player->y][player->x] = 'B';
        }
        else if (game->board[player->y][player->x] == 'B')
        {
            game->board[player->y][player->x] = 'R';
        }
        return;
    }

    if (newX >= 0 && newX < game->width && newY >= 0 && newY < game->height)
    {
        player->x = newX;
        player->y = newY;
    }

    // 모든 클라이언트에 게임 정보를 전송
    send_game_info_to_all_clients(game);
}

// 모든 클라이언트에 게임 정보를 전송하는 함수
void send_game_info_to_all_clients(GameInfo *game)
{
    // 모든 플레이어 돌기
    for (int i = 0; i < game->player_num; i++)
    {
        if (game->players[i].ready && game->players[i].clnt_sd != -1) // -1이면 준비아직
        {
            send_game_info(game->players[i].clnt_sd, game);
        }
    }
}

void *client_handler(void *arg)
{
    ThreadArg *targ = (ThreadArg *)arg;
    int clnt_sd = targ->clnt_sd;
    GameInfo *game = targ->game;
    char buffer[256];
    ssize_t numBytes;

    game->players[targ->p_num].clnt_sd = clnt_sd; // clnt_sd 저장

    // 플레이어 ID -> clnt 전송
    if (write(clnt_sd, &targ->p_num, sizeof(targ->p_num)) <= 0)
    {
        printf("Failed to send player ID to client %d.\n", targ->p_num);
        close(clnt_sd);
        return NULL;
    }

    // 접속 확인을 위해 'y' 입력 받기
    while (1)
    {
        numBytes = read(clnt_sd, buffer, sizeof(buffer)); // from clnt -> 데이터 읽기
        if (numBytes <= 0)
        {
            printf("Client %d disconnected.\n", targ->p_num);
            pthread_mutex_lock(&game->lock);
            game->players[targ->p_num].ready = 0; // clnt 준비되지 않음으로 설정
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
                pthread_cond_broadcast(&game->start_cond); // 모든 clnt에게 게임 시작 알림
            }
            pthread_mutex_unlock(&game->lock);
            break;
        }
    }

    // game 구조체의 데이터에 대한 동시 접근 방지
    pthread_mutex_lock(&game->lock);
    // 모든 플레이어가 준비될 때까지 대기
    while (game->players_ready < game->player_num)
    {
        pthread_cond_wait(&game->start_cond, &game->lock); // 모든 플레이어가 준비될 때까지 대기
    }
    pthread_mutex_unlock(&game->lock); // 준비 다 됐으니까 Unlock

    // 초기 게임 상태 전송
    send_game_info(clnt_sd, game);

    // 게임 시간이 남아있는 동안 명령을 처리
    while (game->play_time > 0)
    {
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

        printf("Client %d command: %c\n", targ->p_num, buffer[0]);

        // 클라이언트 명령을 처리하고 모든 클라이언트에 게임 정보 전송
        pthread_mutex_lock(&game->lock);
        process_player_command(buffer[0], targ->p_num, game);
        pthread_mutex_unlock(&game->lock);
        send_game_info_to_all_clients(game);
    }

    // 게임 종료 후 타일 갯수 전송
    int red_count, blue_count;
    calculate_tile_counts(game, &red_count, &blue_count);
    char winner;
    if (red_count > blue_count)
        winner = 'R';
    else if (blue_count > red_count)
        winner = 'B';
    else
        winner = 'T'; // Tie

    // 타일 갯수와 승자를 클라이언트에 전송
    send_tile_counts(clnt_sd, red_count, blue_count, winner);

    // 클라이언트로부터 종료 확인 메시지를 기다림
    numBytes = read(clnt_sd, buffer, sizeof(buffer)); // 클라이언트로부터 종료 메시지 읽기
    if (numBytes > 0 && buffer[0] == 'q')
    {
        printf("Client %d game end.\n", targ->p_num);
    }

    // 게임 종료 후 소켓 닫기
    close(clnt_sd);
    return NULL;
}

// 게임 상태 업데이트
void update_game_state(GameInfo *game)
{
    pthread_mutex_lock(&game->lock);
    game->play_time--;                   // 게임 시간 감소
    send_game_info_to_all_clients(game); // 모든 클라이언트에 게임 상태 전송
    pthread_mutex_unlock(&game->lock);
}

// 타일 카운트 결과 전송
void send_tile_counts(int clnt_sd, int red_count, int blue_count, char winner)
{
    if (write(clnt_sd, &red_count, sizeof(red_count)) != sizeof(red_count))
    {
        error_handling("Error sending red_count");
    }
    if (write(clnt_sd, &blue_count, sizeof(blue_count)) != sizeof(blue_count))
    {
        error_handling("Error sending blue_count");
    }
    if (write(clnt_sd, &winner, sizeof(winner)) != sizeof(winner))
    {
        error_handling("Error sending winner");
    }
}

// 타일 카운트 계산
void calculate_tile_counts(GameInfo *game, int *red_count, int *blue_count)
{
    *red_count = 0;
    *blue_count = 0;
    for (int i = 0; i < game->height; i++)
    {
        for (int j = 0; j < game->width; j++)
        {
            if (game->board[i][j] == 'R')
            {
                (*red_count)++;
            }
            else if (game->board[i][j] == 'B')
            {
                (*blue_count)++;
            }
        }
    }
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
    GameInfo *game = malloc(sizeof(GameInfo)); // 게임 정보 동적 할당

    // game setting을 위한
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

    // 소켓 생성
    serv_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sd < 0)
        error_handling("Socket creation failed");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(port);

    // 소켓에 주소 할당
    if (bind(serv_sd, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) < 0)
        error_handling("bind() error");

    // 클라이언트 연결 대기 상태로 설정
    if (listen(serv_sd, player_num) < 0)
        error_handling("listen() error");

    // 게임 초기화
    initialize_game(game, width, height, tile_num, play_time, player_num);

    // 각 플레이어마다 하나의 스레드가 필요 -> player_num만큼의 pthread_t 크기를 곱하여 할당
    threads = malloc(player_num * sizeof(pthread_t));
    // 각 스레드가 고유의 인수(ThreadArg)를 가짐 -> player_num만큼의 ThreadArg 크기를 곱하여 할당
    ThreadArg *thread_args = malloc(player_num * sizeof(ThreadArg));
    // clnt 주소 구조체의 크기를 client_addr_size 변수에 저장
    // 이후 accept에서 clnt의 연결 요청을 수락할 때 사용
    client_addr_size = sizeof(client_addr);

    // 각 clnt에 대해 스레드 생성
    for (int i = 0; i < player_num; i++)
    {
        clnt_sd = accept(serv_sd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (clnt_sd < 0)
            error_handling("accept() error");

        printf("Player %d has connected.\n", i);
        thread_args[i].p_num = i;
        thread_args[i].clnt_sd = clnt_sd;
        thread_args[i].game = game;
        pthread_create(&threads[i], NULL, client_handler, &thread_args[i]);
    }

    // 게임 시간이 끝날 때까지 상태 업데이트
    while (game->play_time > 0)
    {
        update_game_state(game);
        sleep(1); // 1초 대기
    }

    // 타일 카운트 계산
    int red_count, blue_count;
    calculate_tile_counts(game, &red_count, &blue_count);
    char winner;
    if (red_count > blue_count)
        winner = 'R';
    else if (blue_count > red_count)
        winner = 'B';
    else
        winner = 'T'; // Tie

    // 타일 카운트 결과를 모든 클라이언트에 전송
    for (int i = 0; i < player_num; i++)
    {
        if (game->players[i].ready && game->players[i].clnt_sd != -1)
        {
            send_tile_counts(game->players[i].clnt_sd, red_count, blue_count, winner);
        }
    }

    // 서버에 타일 카운트 결과 출력
    printf("Red tiles: %d\n", red_count);
    printf("Blue tiles: %d\n", blue_count);

    // 모든 클라이언트로부터 종료 확인 메시지를 기다림
    for (int i = 0; i < player_num; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // 메모리 해제 및 소켓 닫기
    free(threads);
    free(thread_args);
    free(game->board);
    free(game->players);
    free(game);
    close(serv_sd);
    return 0;
}
