#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h> 
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>


#define JEDN_LEKKA     0
#define JEDN_CIEZKA    1
#define JEDN_JAZDA     2
#define JEDN_ROBOTNICY 3

#define CZAS_LEKKA     2
#define CZAS_CIEZKA    3
#define CZAS_JAZDA     5
#define CZAS_ROBOTNICY 2

#define KOSZT_LEKKA     100
#define KOSZT_CIEZKA    250
#define KOSZT_JAZDA     550
#define KOSZT_ROBOTNICY 150

#define ATAK_LEKKA 1
#define ATAK_CIEZKA 1.5
#define ATAK_JAZDA 3.5
#define ATAK_ROBOTNICY 0

#define OBRONA_LEKKA 1.2
#define OBRONA_CIEZKA 3
#define OBRONA_JAZDA 1.2
#define OBRONA_ROBOTNICY 0

#define KANAL_SERWER  1  
#define KANAL_GRACZ_1 10
#define KANAL_GRACZ_2 20

#define ZLECENIE_STAN      10
#define ZLECENIE_PRODUKCJA 20
#define ZLECENIE_ATAK      30
#define WYNIK_WALKI        40
#define INFORMACJA_BRAKU_SUROWCÓW   50
#define PRODUKCJA_TRWA   60
#define NIEPRAWIDLOWY_TYP_JEDNOSTKI 70
#define NIEPRAWIDLOWY_ZLECENIE_ATAK 80
#define ATAK_W_TOKU 90
#define BLAD_ZLECENIA 100

#define SEM_LOCK -1 
#define SEM_UNLOCK 1 

int shmid;
int msgid;
int semid;
pid_t pid;

struct GameMessage {
    long mtype;
    struct {
        int player_id;
        int action;
        int resources;
        int troop[4];
        int details;
    } data;
};

struct Attack{
    int active;
    int attacker;
    int defender;
    int troops[4];
    int time_left;
};

struct ProductionOrder {
    int active; 
    int unit_type; 
    int pending_count; 
    int time_left;
};      

struct Player{
    int id;
    int resources;
    int troops[4]; 
    struct ProductionOrder current_production;
    int points;
};

void blokuj(int semid){
    struct sembuf operacje[1];
    operacje[0].sem_num = 0;
    operacje[0].sem_op = SEM_LOCK;
    operacje[0].sem_flg = 0;

    if (semop(semid, operacje, 1) == -1) {
        perror("Niepowodzenie blokowania semafora.");
    }
}

void odblokuj(int semid){
    struct sembuf operacje[1];
    operacje[0].sem_num = 0;
    operacje[0].sem_op = SEM_UNLOCK;
    operacje[0].sem_flg = 0;

    if (semop(semid, operacje, 1) == -1) {
        perror("Niepowodzenie odblokowania semafora.");
    }
}

void zakoncz(int signal) {
    if (pid == 0) {

        exit(0);
    }
    else {
        printf("Odebrano sygnał zakończenia. Czyszczenie zasobów...\n");
        if(shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("Niepowodzenie usuwania pamięci współdzielonej.");
        }
        else {
            printf("Pamięć współdzielona usunięta pomyślnie.\n");
        }
        if(msgctl(msgid, IPC_RMID, NULL) == -1) {
            perror("Niepowodzenie usuwania kolejki komunikatów.");  
        }
        else {
            printf("Kolejka komunikatów usunięta pomyślnie.\n");
        }
        if (semctl(semid, 0, IPC_RMID) == -1) {
            perror("Niepowodzenie usuwania semafora.");
        }
        else {
            printf("Semafor usunięty pomyślnie.\n");
        }
        exit(0);
    }
}