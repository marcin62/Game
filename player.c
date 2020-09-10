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

const int map_part_szer = 5;
const int map_part_wys = 5;

int baza_flaga, baza_x, baza_y;
int ID_client;

sem_t * connection_manager_sem;

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

//---Struktura bazowa do ktorej podlacza sie klient w celu określenia ID
struct client_id
{
    int ID_client;
};

void * print_game(void * global_data);

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
    //---Podlaczanie do struktury clientID
    int test = 0;
    mvprintw(0, 0, "Oczekiwanie...");
    while (test <= 0)
    {
        test = shm_open("client_id", O_RDWR, 0600);
        sleep(2);
    }
    struct client_id * clientID = (struct client_id*)mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, test, 0);
    if (clientID == MAP_FAILED) 
    {
        perror("mapowanie struktury client_id"); 
        return 0;
    }
    //---Podlaczanie do semaforow żądających ID od serwera
    sem_t * client_id_in_semafor = sem_open("client_id_in_semafor", 0);
    if (client_id_in_semafor == SEM_FAILED) 
    {
        perror("otwieranie semafora client_id_in_semafor"); 
        return 0;
    }
    sem_t * client_id_out_semafor = sem_open("client_id_out_semafor", 0);
    if (client_id_out_semafor == SEM_FAILED) 
    {
        perror("otwieranie semafora client_id_out_semafor"); 
        return 0;
    }
    sem_t * client_left_semafor = sem_open("client_left_semafor", 0);
    if (client_left_semafor == SEM_FAILED) 
    {
        perror("otwieranie semafora client_left_semafor"); 
        return 0;
    }
    //---Żądanie ID od serwera
    int ID_SHM = -1;
    clientID->ID_client = -1;
    sem_post(client_id_out_semafor);
    sem_wait(client_id_in_semafor);
    if (clientID->ID_client >= 0)
    {
        //---Serwer przyznał ID dla klienta
        switch(clientID->ID_client)
        {
            case 0:
                ID_SHM = shm_open("player1_mem", O_RDWR, 0600);
            break;
            case 1:
                ID_SHM = shm_open("player2_mem", O_RDWR, 0600);
            break;
            case 2:
                ID_SHM = shm_open("player3_mem", O_RDWR, 0600);
            break;
            case 3:
                ID_SHM = shm_open("player4_mem", O_RDWR, 0600);
            break;
        }
    }
    else
    {
        mvprintw(0, 0, "Serwer jest pelny");
        refresh(); sleep(5); endwin();
        return 0;
    }
    //---Podlaczanie do wlasciwej struktury
    struct client_dane * global_data;
    if(ID_SHM >= 0)
    {
        global_data = (struct client_dane*)mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, ID_SHM, 0);
        if (global_data == MAP_FAILED) 
        {
            perror("Blad mapowania wlasciwiej struktury klienta");
            endwin(); 
            return 0;
        }
    }
    else
    {
        mvprintw(0, 0, "Blad przyznawania ID_SHM");
        refresh(); sleep(5); endwin();
        return 0;
    }
    ID_client = clientID->ID_client;
    //---Inicjalizacja i uruchamianie watkow
    pthread_t gui_thread;
    pthread_create(&gui_thread, NULL, (void *)print_game, global_data);
    //---Główna pętla gracza
    int loop=1;
    while (loop)
    {
        int przycisk = getch();
        switch(przycisk)
        {
            case 'q':
                loop = 0;    
            break;
            case 'Q':
                loop = 0;
            break;
            case KEY_UP:
                if (global_data->player_dir==WOLNE)
                {
                    global_data->player_dir=GORA;
                }
            break;
            case KEY_DOWN:
                if (global_data->player_dir==WOLNE)
                {
                    global_data->player_dir=DOL;
                }
            break;
            case KEY_LEFT:
                if (global_data->player_dir==WOLNE)
                {
                    global_data->player_dir=LEWO;
                }
            break;
            case KEY_RIGHT:
                if (global_data->player_dir==WOLNE)
                {
                    global_data->player_dir=PRAWO;
                }
            break;
        }
    }
    global_data->x = -4;
    global_data->y = -4;
    sem_post(client_left_semafor);
    endwin();
    return 0;
}

void * print_game(void * global_data)
{
    while (true)
    {
        //---Printowanie mapy 
        int x = ((struct client_dane *)global_data)->x-2;
        int y = ((struct client_dane *)global_data)->y-2;
        int g = 0, k = 0;
        for (int i = y; i < y + map_part_wys; i++)
        {
            for (int j = x; j < x + map_part_szer; j++)
            {
                if (((struct client_dane *)global_data)->map_client[g][k] == '-') 
                {
                    attron(COLOR_PAIR(2));
                    mvprintw(i, j, "%c", ' ');
                }
                if (((struct client_dane *)global_data)->map_client[g][k] == '|')
                {
                    attron(COLOR_PAIR(3));
                    mvprintw(i, j, "%c", '|');
                }
                if (((struct client_dane *)global_data)->map_client[g][k] == '#')
                {
                    attron(COLOR_PAIR(4));
                    mvprintw(i, j, "%c", '#');
                }   
                if (((struct client_dane *)global_data)->map_client[g][k] == 'D')
                {
                    attron(COLOR_PAIR(8));
                    mvprintw(i, j, "%c", 'D');
                }  
                if (((struct client_dane *)global_data)->map_client[g][k] == 'c')
                {
                    attron(COLOR_PAIR(7));
                    mvprintw(i, j, "%c", 'c');
                }   
                if (((struct client_dane *)global_data)->map_client[g][k] == 't')
                {
                    attron(COLOR_PAIR(7));
                    mvprintw(i, j, "%c", 't');
                }   
                if (((struct client_dane *)global_data)->map_client[g][k] == 'T')
                {
                    attron(COLOR_PAIR(7));
                    mvprintw(i, j, "%c", 'T');
                }    
                if (((struct client_dane *)global_data)->map_client[g][k] == '*')
                {
                    attron(COLOR_PAIR(1));
                    mvprintw(i, j, "%c", '*');
                }    
                if (((struct client_dane *)global_data)->map_client[g][k] >= '1' && ((struct client_dane *)global_data)->map_client[g][k] <= '4')
                {
                    attron(COLOR_PAIR(9));
                    mvprintw(i, j, "%c", ((struct client_dane *)global_data)->map_client[g][k]);
                }
                if (((struct client_dane *)global_data)->map_client[g][k] == 'A')
                {
                    attron(COLOR_PAIR(6));
                    mvprintw(i, j, "%c", 'A');
                    baza_flaga = 1;
                    baza_y = i;
                    baza_x = j;
                }   
                if (g == 2 && k == 2)
                {
                    attron(COLOR_PAIR(9));
                    mvprintw(i, j, "%c", ID_client + 1 + '0');
                }
                k++;
            }
            g++;
            k = 0;
        }
        attron(COLOR_PAIR(4));
        mvprintw(0, 54, "Server's PID: %d", 1);
        if (baza_flaga) mvprintw(1, 54, "Campsite X/Y: %02d/%02d", baza_x, baza_y);
        else mvprintw(1, 54, "Campsite X/Y: unknown");
        mvprintw(2, 54, "Round number: %d", (((struct client_dane *)global_data)-> runda));
        mvprintw(5, 54, "Player:");
        mvprintw(6, 54, "Number: %d", ID_client + 1);
        mvprintw(7, 54, "Type: HUMAN");
        mvprintw(8, 54, "X: %d", (((struct client_dane *)global_data)-> x));
        mvprintw(9, 54, "Y: %d", (((struct client_dane *)global_data)-> y));
        mvprintw(10, 54, "Deaths: %d", (((struct client_dane *)global_data)-> death_count));
        mvprintw(11, 54, "Coins found: %d", (((struct client_dane *)global_data)-> coins_on));
        mvprintw(12, 54, "Coins brought: %d", (((struct client_dane *)global_data)-> coins_off));
        mvprintw(16, 54, "Legend:");
        mvprintw(17, 54, "1234 - Players");
        mvprintw(18, 54, "| - Wall:");
        mvprintw(19, 54, "# - Bushes");
        mvprintw(20, 54, "* - Enemy:");
        mvprintw(21, 54, "c - One coin");
        mvprintw(22, 54, "t - Treasure (10 coins)");
        mvprintw(23, 54, "T - Large treasure (50 coins)");
        mvprintw(24, 54, "A - Campsite");
        refresh();
        usleep(250000);
        clear();
    }
    return NULL;
}
