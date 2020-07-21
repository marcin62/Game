#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>

#define max_clients 4

const int max_rows = 25;
const int max_columns = 51;

char map [][51] = {
"|||||||||||||||||||||||||||||||||||||||||||||||||||",
"|---|-------|---#####-------|---------|-------|---|",
"|-|-|||-|||-|||||||||||-|||-|-|||||||-|||-|||||---|",
"|-|---|-|-|-----------|-|-|---|-----|-----|---|---|",
"|-|||-|-|-|||###|||||-|-|-|||||-|||||||||||||-|||-|",
"|-|-|---|-----------|-|-|-##--|-------|-------|-|-|",
"|-|-|||||-|||-|||||||-|-|-|-|||-|||-|||-|||-|-|-|-|",
"|-|---------|-|-------|-|-|-----|---|---|-|-|---|-|",
"|-|-|||||||-|||-|||||||-|||||-|||-|||-|||-|-|||-|-|",
"|-|-|-----|---|---|-----|---|---|---------|-|-|-|-|",
"|-|||-|||-|||-|||-|||-|||-|-|||-|||||||||||-|-|-|-|",
"|-|---|-------|-|---|-----|-|---|-|-------|-|---|-|",
"|-|-||||||#||-|-|||-|||-|||-|||-|-|-|||||-|-|-|||-|",
"|-|----#|---|-|---|---|---|---|---|-|-----|-|-|---|",
"|-|-|-##|-|||-|||-|||-|||||||-|||-|-|||-|||-|-|-|||",
"|-|-|##-|----#--|---|-|--###--|---|---|-----|-|-|-|",
"|-|-|#--|||||||-|-|-|-|-||#||||-|||||-|||||||-|-|-|",
"|-|-|#------|---|-|-|---|-----|---|-|-------##|---|",
"|-|||||||||-|-|||-|||||||-|||||||-|-|||||-|-##|||-|",
"|-|#------|-|-----|-----|-------|---|---|-|-##--|-|",
"|-|-|||||-|-|||||||-|-|||-|||||-|||-|-|-|||#|||||-|",
"|###|-----|---------|-----|##-|-----|-|---|######-|",
"|-|||-|||||||||||||||||||||#|-|||||||-|||-|#----#-|",
"|---|-----------------######|##---------|----##---|",
"|||||||||||||||||||||||||||||||||||||||||||||||||||",
};

sem_t * client_manager_semafor;
sem_t * client_id_in_semafor;
sem_t * client_id_out_semafor;
sem_t * client_left_semafor;
sem_t * refresh_semafor;
sem_t * coord_resetter_semafor;
sem_t * map_part_assigner_semafor;

enum move_dir
{
    GORA,
    DOL,
    LEWO,
    PRAWO,
    WOLNE
};

enum player_type
{
    HUMAN,
    BOT
};

//---Dane pomocnicze serwera w zarzadzaniu klientami
struct client_dane
{ 
    enum move_dir player_dir;
    enum player_type player_t;
    char map_client[5][5];
    unsigned int death_count;
    unsigned int coins_on;
    unsigned int coins_off;
    int x, y;
    int runda;
    int slowed;
    int init_map;
};

struct sakiewka
{
    int x, y;
    int zetony;
};

struct bestia
{
    int x, y;
};

//---Dane pomocnicze serwera w zarzadzaniu gra
struct serwer_dane
{
    unsigned int players_connected[max_clients];
    unsigned int players_count;
    struct client_dane * players_dane[max_clients];
    struct sakiewka tablica_sakiewek[255];
    unsigned int iterator_sakiewek;
    struct bestia tablica_bestii[255];
    unsigned int licznik_bestii;
    unsigned int runda;
};

//---Struktura bazowa do ktorej podlacza sie klient w celu określenia ID
struct client_id
{
    int ID_client;
};
struct client_id * client_id;

void spawn_camp();
int move_ok(int x, int y);
int draw_line_left(int playerx, int beastx, int beasty);
int draw_line_right(int playerx, int beastx, int beasty);
int draw_line_up(int playery, int beastx, int beasty);
int draw_line_down(int playery, int beastx, int beasty);
void * print_game(void * global_data);
void * player_manager(void * global_data);
void * ID_assign_manager(void * global_data);
void * player_left_manager(void * global_data);
void * coord_resetter(void * global_data);
void * map_part_assigner(void * global_data);
void * beast_manager(void * global_data);

int main(void)
{
    srand(time(NULL));
    initscr();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(FALSE);
    start_color();
    init_pair(1, COLOR_RED, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, COLOR_WHITE);
    init_pair(3, COLOR_BLACK, COLOR_BLACK);
    init_pair(4, COLOR_BLACK, COLOR_WHITE);
    init_pair(5, COLOR_WHITE, COLOR_BLACK);
    init_pair(6, COLOR_YELLOW, COLOR_GREEN);
    init_pair(7, COLOR_WHITE, COLOR_YELLOW);
    init_pair(8, COLOR_GREEN, COLOR_YELLOW);
    init_pair(9, COLOR_WHITE, COLOR_MAGENTA);
    //---Inicjalizacja pamieci struktury przyznajacej ID klientom
    int client_id_mem = shm_open("client_id", O_CREAT | O_RDWR, 0600);
    if (client_id_mem < 0) 
    {
        perror("blad otwarcia pamieci client_id" + __LINE__); 
        return 1;
    }
    int client_id_error = ftruncate(client_id_mem, sizeof(struct client_id));
    if (client_id_error < 0) 
    {
        perror("blad przycinania pamieci client_id" + __LINE__); 
        return 1;
    }
    client_id = (struct client_id*)mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, client_id_mem, 0);
    if (client_id == MAP_FAILED) 
    {
        perror("blad mmapowania pamieci client_id" + __LINE__); 
        return 1;
    }
    //---Inicjalizacja pamieci graczy
    //---Gracz pierwszy
    int player1_mem = shm_open("player1_mem", O_CREAT | O_RDWR, 0600);
    if (player1_mem < 0) 
    {
        perror("blad otwarcia pamieci pierwszego klienta" + __LINE__); 
        return 1;
    }
    int player1_error = ftruncate(player1_mem, sizeof(struct client_dane));
    if (player1_error < 0) 
    {
        perror("blad przycinania pamieci pierwszego klienta" + __LINE__); 
        return 1;
    }
    struct client_dane * player1 = (struct client_dane*)mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, player1_mem, 0);
    if (player1 == MAP_FAILED) 
    {
        perror("blad mmapowania pamieci pierwszego klienta" + __LINE__); 
        return 1;
    }
    //---Gracz drugi
    int player2_mem = shm_open("player2_mem", O_CREAT | O_RDWR, 0600);
    if (player2_mem < 0) 
    {
        perror("blad otwarcia pamieci drugiego klienta" + __LINE__); 
        return 1;
    }
    int player2_error = ftruncate(player2_mem, sizeof(struct client_dane));
    if (player2_error < 0) 
    {
        perror("blad przycinania pamieci drugiego klienta" + __LINE__); 
        return 1;
    }
    struct client_dane * player2 = (struct client_dane*)mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, player2_mem, 0);
    if (player2 == MAP_FAILED) 
    {
        perror("blad mmapowania pamieci drugiego klienta" + __LINE__); 
        return 1;
    }
    //---Gracz trzeci
    int player3_mem = shm_open("player3_mem", O_CREAT | O_RDWR, 0600);
    if (player3_mem < 0) 
    {
        perror("blad otwarcia pamieci trzeciego klienta" + __LINE__); 
        return 1;
    }
    int player3_error = ftruncate(player3_mem, sizeof(struct client_dane));
    if (player3_error < 0) 
    {
        perror("blad przycinania pamieci trzeciego klienta" + __LINE__); 
        return 1;
    }
    struct client_dane * player3 = (struct client_dane*)mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, player3_mem, 0);
    if (player3 == MAP_FAILED) 
    {
        perror("blad mmapowania pamieci trzeciego klienta" + __LINE__); 
        return 1;
    }
    //---Gracz czwarty
    int player4_mem = shm_open("player4_mem", O_CREAT | O_RDWR, 0600);
    if (player4_mem < 0) 
    {
        perror("blad otwarcia pamieci czwartego klienta" + __LINE__); 
        return 1;
    }
    int player4_error = ftruncate(player4_mem, sizeof(struct client_dane));
    if (player4_error < 0) 
    {
        perror("blad przycinania pamieci czwartego klienta" + __LINE__); 
        return 1;
    }
    struct client_dane * player4 = (struct client_dane*)mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, player4_mem, 0);
    if (player4 == MAP_FAILED) 
    {
        perror("blad mmapowania pamieci czwartego klienta" + __LINE__); 
        return 1;
    }
    //---Inicjalizacja stuktury serwer_dane
    struct serwer_dane global_data;
    global_data.iterator_sakiewek = 0;
    global_data.players_count = 0;
    global_data.runda=0;
    global_data.licznik_bestii=0;
    spawn_camp();
    for (int i = 0; i < max_clients; i++)
    {
        global_data.players_connected[i] = 0;
    }
    global_data.players_dane[0] = player1;
    global_data.players_dane[1] = player2;
    global_data.players_dane[2] = player3;
    global_data.players_dane[3] = player4;
    //---Inicjalizacja struktur graczy    
    for (int i = 0; i < max_clients; i++)
    {
        (global_data.players_dane[i])->player_dir = WOLNE;
        (global_data.players_dane[i])->player_t = HUMAN;
        (global_data.players_dane[i])->death_count = 0;
        (global_data.players_dane[i])->coins_off = 0;
        (global_data.players_dane[i])->coins_on = 0;
        (global_data.players_dane[i])->runda = 0;
        (global_data.players_dane[i])->slowed = 0;
        (global_data.players_dane[i])->x = -5;
        (global_data.players_dane[i])->y = -5;
        (global_data.players_dane[i])->init_map = 0;
    }
    //---Inicjalizacja i uruchamianie watkow
    pthread_t gui_thread;
    pthread_create(&gui_thread, NULL, (void *)print_game, &global_data);
    pthread_t id_manager_thread;
    pthread_create(&id_manager_thread, NULL, (void *)ID_assign_manager, &global_data);
    pthread_t client_left_manager_thread;
    pthread_create(&client_left_manager_thread, NULL, (void *)player_left_manager, &global_data);
    pthread_t player_manager_thread;
    pthread_create(&player_manager_thread, NULL, (void *)player_manager, &global_data);
    pthread_t coord_resetter_thread;
    pthread_create(&coord_resetter_thread, NULL, (void *)coord_resetter, &global_data);
    pthread_t map_part_assigner_thread;
    pthread_create(&map_part_assigner_thread, NULL, (void *)map_part_assigner, &global_data);
    pthread_t beast_manager_thread;
    pthread_create(&beast_manager_thread, NULL, (void *)beast_manager, &global_data);
    //---Inicjalizacja i uruchamianie semaforow
    client_id_in_semafor = sem_open("client_id_in_semafor", O_CREAT, 0600, 0);
    if (client_id_in_semafor == SEM_FAILED) 
    {
        perror("client_id_in_semafor linia:" + __LINE__); 
        return -1;
    }
    client_id_out_semafor = sem_open("client_id_out_semafor", O_CREAT, 0600, 0);
    if (client_id_out_semafor == SEM_FAILED) 
    {
        perror("client_id_out_semafor linia:" + __LINE__); 
        return -1;
    }
    client_left_semafor = sem_open("client_left_semafor", O_CREAT, 0600, 0);
    if (client_left_semafor == SEM_FAILED) 
    {
        perror("client_left_semafor linia:" + __LINE__); 
        return -1;
    }
    refresh_semafor = sem_open("refresh_semafor", O_CREAT, 0600, 0);
    if (refresh_semafor == SEM_FAILED) 
    {
        perror("refresh_semafor linia:" + __LINE__); 
        return -1;
    }
    coord_resetter_semafor = sem_open("coord_resetter_semafor", O_CREAT, 0600, 0);
    if (coord_resetter_semafor == SEM_FAILED) 
    {
        perror("coord_resetter_semafor linia:" + __LINE__); 
        return -1;
    }
    map_part_assigner_semafor = sem_open("map_part_assigner_semafor", O_CREAT, 0600, 0);
    if (map_part_assigner_semafor == SEM_FAILED) 
    {
        perror("map_part_assigner_semafor linia:" + __LINE__); 
        return -1;
    }
    //---Główna pętla servera
    int loop=1;
    while (loop)
    {
        int przycisk;
        przycisk = getch();
        int x, y, flag = 1;
        switch(przycisk)
        {
            case 'q':
                loop = 0;    
            break;
            case 'Q':
                loop = 0;
            break;
            case 'c':
                while (flag)
                {
                    x = rand()%max_columns;
                    y = rand()%max_rows;
                    if (map[y][x] == '-') flag = 0;
                }
                map[y][x] = 'c';
            break;
            case 't':
                while (flag)
                {
                    x = rand()%max_columns;
                    y = rand()%max_rows;
                    if (map[y][x] == '-') flag = 0;
                }
                map[y][x] = 't';
            break;
            case 'T':
                while (flag)
                {
                    x = rand()%max_columns;
                    y = rand()%max_rows;
                    if (map[y][x] == '-') flag = 0;
                }
                map[y][x] = 'T';
            break;
            case 'b':
            case 'B':
                while (flag)
                {
                    x = rand()%max_columns;
                    y = rand()%max_rows;
                    if (map[y][x] == '-') flag = 0;
                }
                global_data.tablica_bestii[global_data.licznik_bestii].x = x;
                global_data.tablica_bestii[global_data.licznik_bestii].y = y;
                global_data.licznik_bestii++;
            break;
        }
    }
    //---Zwalnianie pamięci
    //---Client_ID
    munmap(client_id, sizeof(struct client_id));
    close(client_id_mem);
    shm_unlink("client_id");
    //---Gracz pierwszy
    munmap(player1, sizeof(struct client_dane));
    close(player1_mem);
    shm_unlink("player1_mem");
    //---Gracz drugi
    munmap(player2, sizeof(struct client_dane));
    close(player2_mem);
    shm_unlink("player2_mem");
    //---Gracz trzeci
    munmap(player3, sizeof(struct client_dane));
    close(player3_mem);
    shm_unlink("player3_mem");
    //---Gracz czwarty
    munmap(player4, sizeof(struct client_dane));
    close(player4_mem);
    shm_unlink("player4_mem");
    //Zwalnianie semaforów
    sem_close(client_id_in_semafor);
    sem_unlink("client_id_in_semafor");
    sem_close(client_id_out_semafor);
    sem_unlink("client_id_out_semafor");
    sem_close(client_left_semafor);
    sem_unlink("client_left_semafor");
    sem_close(refresh_semafor);
    sem_unlink("refresh_semafor");
    sem_close(coord_resetter_semafor);
    sem_unlink("coord_resetter_semafor");
    sem_close(map_part_assigner_semafor);
    sem_unlink("map_part_assigner_semafor");
    endwin();
    return 0;
}

void * print_game(void * global_data)
{
    while (true)
    {
        sem_wait(refresh_semafor);
        //---Printowanie mapy 
        clear();
        for (int i = 0; i < max_rows; i++)
        {
            for (int j = 0; j < max_columns; j++)
            {
                if (map[i][j] == '-') 
                {
                    attron(COLOR_PAIR(2));
                    mvprintw(i, j, "%c", ' ');
                }
                if (map[i][j] == '|')
                {
                    attron(COLOR_PAIR(3));
                    mvprintw(i, j, "%c", map[i][j]);
                }
                if (map[i][j] == '#')
                {
                    attron(COLOR_PAIR(4));
                    mvprintw(i, j, "%c", map[i][j]);
                }   
                if (map[i][j] == 'c')
                {
                    attron(COLOR_PAIR(7));
                    mvprintw(i, j, "%c", map[i][j]);
                }   
                if (map[i][j] == 't')
                {
                    attron(COLOR_PAIR(7));
                    mvprintw(i, j, "%c", map[i][j]);
                }   
                if (map[i][j] == 'T')
                {
                    attron(COLOR_PAIR(7));
                    mvprintw(i, j, "%c", map[i][j]);
                }   
                if (map[i][j] == 'A')
                {
                    attron(COLOR_PAIR(6));
                    mvprintw(i, j, "%c", map[i][j]);
                }   
            }
        }
        //---Printowanie graczy
        for (int i = 0; i < max_clients; i++)
        {
            if (((struct serwer_dane *)global_data)->players_connected[i])
            {
                attron(COLOR_PAIR(9));
                mvprintw(((struct serwer_dane *)global_data)->players_dane[i]->y, ((struct serwer_dane *)global_data)->players_dane[i]->x, "%d", i + 1);
            }
        }
        //---Printowanie sakiewek
        for (int g = 0; g < ((struct serwer_dane*)global_data)->iterator_sakiewek; g++)
        {
            int x = ((struct serwer_dane*)global_data)->tablica_sakiewek[g].x;
            int y = ((struct serwer_dane*)global_data)->tablica_sakiewek[g].y;
            attron(COLOR_PAIR(8));
            mvprintw(y, x, "%c", 'D');
        }
        //---Printowanie bestii
        for (int g = 0; g < ((struct serwer_dane*)global_data)->licznik_bestii; g++)
        {
            int x = ((struct serwer_dane*)global_data)->tablica_bestii[g].x;
            int y = ((struct serwer_dane*)global_data)->tablica_bestii[g].y;
            attron(COLOR_PAIR(1));
            mvprintw(y, x, "%c", '*');
        }
        //Stats and info
        attron(COLOR_PAIR(4));
        mvprintw(1, 54, "Server's PID: %d", 1);
        mvprintw(2, 54, "Campsite X/Y: %02d/%02d", 0, 0);
        mvprintw(3, 54, "Round number: %u", ((struct serwer_dane *)global_data)->runda);
        mvprintw(5, 54, "Parameters:");
        mvprintw(5, 69, "Player1:");
        mvprintw(5, 79, "Player2:");
        mvprintw(5, 89, "Player3:");
        mvprintw(5, 99, "Player4:");
        mvprintw(7, 55, "PID: ");
        for (int i = 0; i < max_clients; i++)
        {
            if (((struct serwer_dane *)global_data)->players_connected[i])
            {
                mvprintw(7, 70 + i * 10, "%d", 0);
            }
            else
            {
                mvprintw(7, 70 + i * 10, "-");
            }
        }
        mvprintw(8, 55, "TYPE: ");
        for (int i = 0; i < max_clients; i++)
        {
            if (((struct serwer_dane *)global_data)->players_connected[i])
            {
                if (((struct serwer_dane *)global_data)->players_dane[i]->player_t==HUMAN) mvprintw(8, 70 + i * 10, "HUMAN");
                else mvprintw(8 ,70 + i * 10, "BOT");
            }
            else
            {
                mvprintw(8, 70 + i * 10, "-");
            }
        }
        mvprintw(9, 55, "CURR X/Y: ");
        for (int i = 0; i < max_clients; i++)
        {
            if (((struct serwer_dane *)global_data)->players_connected[i])
            {
                mvprintw(9, 70 + i * 10, "%02d/%02d", ((struct serwer_dane *)global_data)->players_dane[i]->x,((struct serwer_dane *)global_data)->players_dane[i]->y);
            }
            else
            {
                mvprintw(9, 70 + i * 10, "--/--");
            }
        }
        mvprintw(10, 55, "DEATHS: ");
        for (int i = 0; i < max_clients; i++)
        {
            if (((struct serwer_dane *)global_data)->players_connected[i])
            {
                mvprintw(10, 70 + i * 10, "%d",  ((struct serwer_dane *)global_data)->players_dane[i]->death_count);
            }
            else
            {
                mvprintw(10, 70 + i * 10, "-");
            }
        }
        mvprintw(11, 54, "Coins");
        mvprintw(12, 55, "CARRIED: ");
        mvprintw(13, 55, "BROUGHT: ");
        for (int i = 0; i < max_clients; i++)
        {        
            if (((struct serwer_dane *)global_data)->players_connected[i])
            {
                mvprintw(12, 70 + i * 10, "%d",  ((struct serwer_dane *)global_data)->players_dane[i]->coins_on);
                mvprintw(13, 70 + i * 10, "%d",  ((struct serwer_dane *)global_data)->players_dane[i]->coins_off);
            }
            else
            {
                mvprintw(12, 70 + i * 10, "-");
                mvprintw(13, 70 + i * 10, "-");
            }
        }
        mvprintw(17, 54, "Legend:");
        mvprintw(18, 54, "1234 - Players");
        mvprintw(19, 54, "| - Wall:");
        mvprintw(20, 54, "# - Bushes");
        mvprintw(21, 54, "* - Enemy:");
        mvprintw(22, 54, "c - One coin");
        mvprintw(23, 54, "t - Treasure (10 coins)");
        mvprintw(24, 54, "T - Large treasure (50 coins)");
        mvprintw(25, 54, "A - Campsite");
        refresh();
    }
    return NULL;
}

void * coord_resetter(void * global_data)
{
    while (true)
    {
        sem_wait(coord_resetter_semafor);
        for (int i =0; i < max_clients; i++)
        {
            if (((struct serwer_dane*)global_data)->players_connected[i])
            {
                if (((struct serwer_dane*)global_data)->players_dane[i]->x == -5 && ((struct serwer_dane*)global_data)->players_dane[i]->y == -5)
                {
                    int flag = 1;
                    while (flag)
                    {
                        ((struct serwer_dane*)global_data)->players_dane[i]->x = rand()%max_columns;
                        ((struct serwer_dane*)global_data)->players_dane[i]->y = rand()%max_rows;
                        if (map[((struct serwer_dane*)global_data)->players_dane[i]->y][((struct serwer_dane*)global_data)->players_dane[i]->x] == '-') flag = 0;
                    }
                    break;
                }
            }
        }
    }
    return NULL;
}

void * map_part_assigner(void * global_data)
{
    while (true)
    {
        sem_wait(map_part_assigner_semafor);
        for (int i =0; i < max_clients; i++)
        {
            if (((struct serwer_dane*)global_data)->players_connected[i])
            {
                if (((struct serwer_dane*)global_data)->players_dane[i]->init_map)
                {
                    ((struct serwer_dane*)global_data)->players_dane[i]->init_map = 0;
                    for (int j = 0; j < 5; j++)
                    {
                        for (int k = 0; k < 5; k++)
                        {
                            int mapx = ((struct serwer_dane*)global_data)->players_dane[i]->x + k - 2;
                            int mapy = ((struct serwer_dane*)global_data)->players_dane[i]->y + j - 2;
                            if (mapx < 0) mapx = 0;
                            if (mapy < 0) mapy = 0;
                            if (mapx > max_columns) mapx = max_columns-1;
                            if (mapy > max_rows) mapy = max_rows-1;
                            ((struct serwer_dane*)global_data)->players_dane[i]->map_client[j][k] = map[mapy][mapx];
                            //---Sprawdzanie czy wokol gracza sa inni gracze
                            for (int g = 0; g < max_clients; g++)
                            {
                                if (((struct serwer_dane*)global_data)->players_connected[g] && g != i)
                                {
                                    int playerx=((struct serwer_dane*)global_data)->players_dane[g]->x;
                                    int playery=((struct serwer_dane*)global_data)->players_dane[g]->y;
                                    if (mapx == playerx && mapy == playery) ((struct serwer_dane*)global_data)->players_dane[i]->map_client[j][k] = (g+1) + '0';
                                }
                            }
                            //---Sprawdzanie czy wokol gracza sa sakiewki
                            for (int g = 0; g < ((struct serwer_dane*)global_data)->iterator_sakiewek; g++)
                            {
                                int x = ((struct serwer_dane*)global_data)->tablica_sakiewek[g].x;
                                int y = ((struct serwer_dane*)global_data)->tablica_sakiewek[g].y;
                                if (mapx == x && mapy==y) ((struct serwer_dane*)global_data)->players_dane[i]->map_client[j][k] = 'D';
                            }
                            //---Sprawdzanie czy wokol gracza sa bestie
                            for (int g = 0; g < ((struct serwer_dane*)global_data)->licznik_bestii; g++)
                            {
                                int x = ((struct serwer_dane*)global_data)->tablica_bestii[g].x;
                                int y = ((struct serwer_dane*)global_data)->tablica_bestii[g].y;
                                if (mapx == x && mapy==y) ((struct serwer_dane*)global_data)->players_dane[i]->map_client[j][k] = '*';
                            }
                        }
                    }
                    break;
                }
            }
        }

    }
    return NULL;
}

void * ID_assign_manager(void * global_data)
{
    while (1)
    {
        sem_wait(client_id_out_semafor);
        for (int i = 0; i < max_clients; i++)
        {
            if (((struct serwer_dane*)global_data)->players_connected[i]==0)
            {
                client_id->ID_client=i;
                ((struct serwer_dane *)global_data)->players_count++;
                ((struct serwer_dane*)global_data)->players_connected[i]=1;
                ((struct serwer_dane*)global_data)->players_dane[i]->x = -5;
                ((struct serwer_dane*)global_data)->players_dane[i]->y = -5;
                ((struct serwer_dane*)global_data)->players_dane[i]->init_map = 1;
                
                sem_post(coord_resetter_semafor);
                usleep(100);
                sem_post(map_part_assigner_semafor);
                break;
            }
        }
        sem_post(client_id_in_semafor);
    }
    return NULL;
}

void * player_manager(void * global_data)
{
    while (true)
    {
        for (int i = 0; i < max_clients; i++)
        {
            if (((struct serwer_dane*)global_data)->players_connected[i])
            {
                //---Sprawdzanie krzakow
                if (((struct serwer_dane*)global_data)->players_dane[i]->slowed)
                {
                    ((struct serwer_dane*)global_data)->players_dane[i]->slowed = 0;
                    continue;
                }
                //---Przemieszczanie graczy
                if (((struct serwer_dane*)global_data)->players_dane[i]->player_dir!=WOLNE)
                {
                    if (((struct serwer_dane*)global_data)->players_dane[i]->player_dir==GORA)
                    {
                        int nowy_x = ((struct serwer_dane*)global_data)->players_dane[i]->x;
                        int nowy_y = ((struct serwer_dane*)global_data)->players_dane[i]->y-1;
                        if (move_ok(nowy_x, nowy_y))
                        {
                            ((struct serwer_dane*)global_data)->players_dane[i]->x = nowy_x;
                            ((struct serwer_dane*)global_data)->players_dane[i]->y = nowy_y;
                        }
                    }
                    if (((struct serwer_dane*)global_data)->players_dane[i]->player_dir==DOL)
                    {
                        int nowy_x = ((struct serwer_dane*)global_data)->players_dane[i]->x;
                        int nowy_y = ((struct serwer_dane*)global_data)->players_dane[i]->y+1;
                        if (move_ok(nowy_x, nowy_y))
                        {
                            ((struct serwer_dane*)global_data)->players_dane[i]->x = nowy_x;
                            ((struct serwer_dane*)global_data)->players_dane[i]->y = nowy_y;
                        }
                    }
                    if (((struct serwer_dane*)global_data)->players_dane[i]->player_dir==LEWO)
                    {
                        int nowy_x = ((struct serwer_dane*)global_data)->players_dane[i]->x-1;
                        int nowy_y = ((struct serwer_dane*)global_data)->players_dane[i]->y;
                        if (move_ok(nowy_x, nowy_y))
                        {
                            ((struct serwer_dane*)global_data)->players_dane[i]->x = nowy_x;
                            ((struct serwer_dane*)global_data)->players_dane[i]->y = nowy_y;
                        }
                    }
                    if (((struct serwer_dane*)global_data)->players_dane[i]->player_dir==PRAWO)   
                    {
                        int nowy_x = ((struct serwer_dane*)global_data)->players_dane[i]->x+1;
                        int nowy_y = ((struct serwer_dane*)global_data)->players_dane[i]->y;
                        if (move_ok(nowy_x, nowy_y))
                        {
                            ((struct serwer_dane*)global_data)->players_dane[i]->x = nowy_x;
                            ((struct serwer_dane*)global_data)->players_dane[i]->y = nowy_y;
                        }
                    }  
                    if (map[((struct serwer_dane*)global_data)->players_dane[i]->y][((struct serwer_dane*)global_data)->players_dane[i]->x] == '#')
                    {
                        ((struct serwer_dane*)global_data)->players_dane[i]->slowed = 1;
                    }      
                    ((struct serwer_dane*)global_data)->players_dane[i]->player_dir=WOLNE;
                }
            }
        }
        for (int i = 0; i < max_clients; i++)
        {
            if (((struct serwer_dane*)global_data)->players_connected[i])
            {
                //---Sprawdzanie czy klient wszedl w sakiewke
                int clientx = ((struct serwer_dane*)global_data)->players_dane[i]->x;
                int clienty = ((struct serwer_dane*)global_data)->players_dane[i]->y;
                for (int it = 0; it < ((struct serwer_dane*)global_data)->iterator_sakiewek; it++)
                {
                    int sakx = ((struct serwer_dane*)global_data)->tablica_sakiewek[it].x;
                    int saky = ((struct serwer_dane*)global_data)->tablica_sakiewek[it].y;
                    if (clientx==sakx && clienty==saky)
                    {
                        ((struct serwer_dane*)global_data)->iterator_sakiewek--;
                        ((struct serwer_dane*)global_data)->players_dane[i]->coins_on += ((struct serwer_dane*)global_data)->tablica_sakiewek[it].zetony;
                        break;
                    }
                }
                //---Odswiezanie mapy kazdego podlaczonego klienta
                ((struct serwer_dane*)global_data)->players_dane[i]->init_map=1;
                sem_post(map_part_assigner_semafor);
            }
        }
        //---Sprawdzanie kolizji
        for (int i = 0; i < max_clients; i++)
        {
            if (((struct serwer_dane*)global_data)->players_connected[i])
            {
                int x = ((struct serwer_dane*)global_data)->players_dane[i]->x;
                int y = ((struct serwer_dane*)global_data)->players_dane[i]->y;
                //---Sprawdzanie kolizji z zetonami
                if (map[y][x] == 'c')
                {
                    map[y][x] = '-';
                    ((struct serwer_dane*)global_data)->players_dane[i]->coins_on+=1;
                }
                if (map[y][x] == 't')
                {
                    map[y][x] = '-';
                    ((struct serwer_dane*)global_data)->players_dane[i]->coins_on+=10;
                }                
                if (map[y][x] == 'T')
                {
                    map[y][x] = '-';
                    ((struct serwer_dane*)global_data)->players_dane[i]->coins_on+=50;
                }
                if (map[y][x] == 'A')
                {
                    ((struct serwer_dane*)global_data)->players_dane[i]->coins_off+=((struct serwer_dane*)global_data)->players_dane[i]->coins_on;
                    ((struct serwer_dane*)global_data)->players_dane[i]->coins_on = 0;
                }
                //---Sprawdzanie kolizji z innymi graczami
                for (int g = 0; g < max_clients; g++)
                {
                    if (((struct serwer_dane*)global_data)->players_connected[g] && g != i)
                    {
                        int playerx=((struct serwer_dane*)global_data)->players_dane[g]->x;
                        int playery=((struct serwer_dane*)global_data)->players_dane[g]->y;
                        if (x == playerx && y == playery) //---Nastapila kolizja
                        {
                            //---Tworzenie sakiewek
                            int coins_sum = ((struct serwer_dane*)global_data)->players_dane[i]->coins_on + ((struct serwer_dane*)global_data)->players_dane[g]->coins_on;
                            ((struct serwer_dane*)global_data)->tablica_sakiewek[((struct serwer_dane*)global_data)->iterator_sakiewek].x = playerx;
                            ((struct serwer_dane*)global_data)->tablica_sakiewek[((struct serwer_dane*)global_data)->iterator_sakiewek].y = playery;
                            ((struct serwer_dane*)global_data)->tablica_sakiewek[((struct serwer_dane*)global_data)->iterator_sakiewek].zetony = coins_sum;
                            if (((struct serwer_dane*)global_data)->iterator_sakiewek < 255) ((struct serwer_dane*)global_data)->iterator_sakiewek++;
                            //---Resetowanie pozycji graczy i inkrementacja smierci
                            ((struct serwer_dane*)global_data)->players_dane[i]->x = -5;
                            ((struct serwer_dane*)global_data)->players_dane[i]->y = -5;
                            ((struct serwer_dane*)global_data)->players_dane[i]->init_map = 1;
                            ((struct serwer_dane*)global_data)->players_dane[i]->death_count++;
                            ((struct serwer_dane*)global_data)->players_dane[i]->coins_on = 0;
                            sem_post(coord_resetter_semafor);
                            usleep(100);
                            sem_post(map_part_assigner_semafor);
                            ((struct serwer_dane*)global_data)->players_dane[g]->x = -5;
                            ((struct serwer_dane*)global_data)->players_dane[g]->y = -5;
                            ((struct serwer_dane*)global_data)->players_dane[g]->init_map = 1;
                            ((struct serwer_dane*)global_data)->players_dane[g]->death_count++;
                            ((struct serwer_dane*)global_data)->players_dane[g]->coins_on = 0;
                            sem_post(coord_resetter_semafor);
                            usleep(100);
                            sem_post(map_part_assigner_semafor);
                        }
                    }
                }
                //---Sprawdzanie kolizji z bestiami
                for (int g = 0; g < ((struct serwer_dane*)global_data)->licznik_bestii; g++)
                {
                    int beastx= ((struct serwer_dane*)global_data)->tablica_bestii[g].x;
                    int beasty= ((struct serwer_dane*)global_data)->tablica_bestii[g].y;
                    if (x == beastx && y == beasty) //---Nastapila kolizja
                    {
                        //---Tworzenie sakiewek
                        int coins_sum = ((struct serwer_dane*)global_data)->players_dane[i]->coins_on;
                        ((struct serwer_dane*)global_data)->tablica_sakiewek[((struct serwer_dane*)global_data)->iterator_sakiewek].x = x;
                        ((struct serwer_dane*)global_data)->tablica_sakiewek[((struct serwer_dane*)global_data)->iterator_sakiewek].y = y;
                        ((struct serwer_dane*)global_data)->tablica_sakiewek[((struct serwer_dane*)global_data)->iterator_sakiewek].zetony = coins_sum;
                        if (((struct serwer_dane*)global_data)->iterator_sakiewek < 255) ((struct serwer_dane*)global_data)->iterator_sakiewek++;
                        //---Resetowanie pozycji gracza i inkrementacja smierci
                        ((struct serwer_dane*)global_data)->players_dane[i]->x = -5;
                        ((struct serwer_dane*)global_data)->players_dane[i]->y = -5;
                        ((struct serwer_dane*)global_data)->players_dane[i]->init_map = 1;
                        ((struct serwer_dane*)global_data)->players_dane[i]->death_count++;
                        ((struct serwer_dane*)global_data)->players_dane[i]->coins_on = 0;
                        sem_post(coord_resetter_semafor);
                        usleep(100);
                        sem_post(map_part_assigner_semafor);
                    }
                }
            }
        }
        if (((struct serwer_dane *)global_data)->players_count>0)
        {
            ((struct serwer_dane *)global_data)->runda++;
            for (int i = 0; i < max_clients; i++)
            {
                ((struct serwer_dane *)global_data)->players_dane[i]->runda = ((struct serwer_dane *)global_data)->runda;
            }
        }
        else ((struct serwer_dane *)global_data)->runda = 0;
        sem_post(refresh_semafor);
        sleep(1);
    }

    return NULL;
}

int move_ok(int x, int y)
{
    return (map[y][x]!='|');
}

void * player_left_manager(void * global_data)
{
    while (true)
    {
        sem_wait(client_left_semafor);
        for (int i = 0; i < max_clients; i++)
        {
            struct client_dane * client = ((struct serwer_dane *)global_data)->players_dane[i];
            if (client->x == -4 && client ->y == -4)
            {
                client->x = -5;
                client->y = -5;
                client->coins_off=0;
                client->coins_on=0;
                client->death_count=0;
                client->player_dir=WOLNE;
                client->runda=0;
                ((struct serwer_dane *)global_data)->players_connected[i]=0;
                ((struct serwer_dane *)global_data)->players_count--;
            }
        }
    }
    return NULL;
}

void spawn_camp()
{
    int x, y;
    while (1)
    {
        x = rand()%max_columns;
        y = rand()%max_rows;
        if (map[y][x] == '-') break;
    }
    map[y][x] = 'A';
}

void * beast_manager(void * global_data)
{
    while (true)
    {
        for (int i = 0; i < ((struct serwer_dane *)global_data)->licznik_bestii; i++)
        {
            //---Iterowanie po każdej z bestii i sprawdzanie czy widzi gracza
            int beastx = ((struct serwer_dane *)global_data)->tablica_bestii[i].x;
            int beasty = ((struct serwer_dane *)global_data)->tablica_bestii[i].y;
            for (int j = 0; j < max_clients; j++)
            {
                if (((struct serwer_dane *)global_data)->players_connected[j])
                {
                    int playerx = ((struct serwer_dane *)global_data)->players_dane[j]->x;
                    int playery = ((struct serwer_dane *)global_data)->players_dane[j]->y;
                    //---Wykrywanie gracza w zasiegu bestii
                    if ((playerx >= beastx-2 && playerx <= beastx+2) && (playery >= beasty-2 && playery <= beasty+2))
                    {
                        //---Rysowanie lini z bestii do gracza, w celu sprawdzenia widoczności
                        if (playery == beasty)
                        {
                            int crossed_left = draw_line_left(playerx, beastx, beasty);
                            int crossed_right = draw_line_right(playerx, beastx, beasty);
                            if (crossed_left)
                            {
                                int new_x = ((struct serwer_dane *)global_data)->tablica_bestii[i].x-1;
                                ((struct serwer_dane *)global_data)->tablica_bestii[i].x=new_x;
                            }
                            if (crossed_right)
                            {
                                int new_x = ((struct serwer_dane *)global_data)->tablica_bestii[i].x+1;
                                ((struct serwer_dane *)global_data)->tablica_bestii[i].x=new_x;
                            }
                        }
                        if (playerx == beastx)
                        {
                            int crossed_up = draw_line_up(playery, beastx, beasty);
                            int crossed_down = draw_line_down(playery, beastx, beasty);   
                            if (crossed_down)
                            {
                                int new_y = ((struct serwer_dane *)global_data)->tablica_bestii[i].y+1;
                                ((struct serwer_dane *)global_data)->tablica_bestii[i].y=new_y;
                            }
                            if (crossed_up)
                            {
                                int new_y = ((struct serwer_dane *)global_data)->tablica_bestii[i].y-1;
                                ((struct serwer_dane *)global_data)->tablica_bestii[i].y=new_y;
                            }
                        }
                    }
                }
            }
        }
        sleep(1);
    }
    return NULL;
}

int draw_line_left(int playerx, int beastx, int beasty)
{
    if (playerx < 0 || playerx > max_columns) return 0;
    if (beastx < 0 || beastx > max_columns) return 0;
    if (beasty < 0 || beasty > max_rows) return 0;
    if (playerx <= beastx)
    {
        while (playerx <= beastx)
        {
            if (playerx == beastx) return 1;
            beastx--;
            if (map[beasty][beastx] == '|') return 0;
        }
    }
    return 0;
}

int draw_line_right(int playerx, int beastx, int beasty)
{
    if (playerx < 0 || playerx > max_columns) return 0;
    if (beastx < 0 || beastx > max_columns) return 0;
    if (beasty < 0 || beasty > max_rows) return 0;
    if (playerx >= beastx)
    {
        while (playerx >= beastx)
        {
            if (playerx == beastx) return 1;
            beastx++;
            if (map[beasty][beastx] == '|') return 0;
        }
    }
    return 0;
}

int draw_line_up(int playery, int beastx, int beasty)
{
    if (playery < 0 || playery > max_rows) return 0;
    if (beastx < 0 || beastx > max_columns) return 0;
    if (beasty < 0 || beasty > max_rows) return 0;
    if (playery <= beasty)
    {
        while (playery <= beasty)
        {
            if (playery == beasty) return 1;
            beasty--;
            if (map[beasty][beastx] == '|') return 0;
        }
    }
    return 0;
}

int draw_line_down(int playery, int beastx, int beasty)
{
    if (playery < 0 || playery > max_rows) return 0;
    if (beastx < 0 || beastx > max_columns) return 0;
    if (beasty < 0 || beasty > max_rows) return 0;
    if (playery >= beasty)
    {
        while (playery >= beasty)
        {
            if (playery == beasty) return 1;
            beasty++;
            if (map[beasty][beastx] == '|') return 0;
        }
    }
    return 0;
}
