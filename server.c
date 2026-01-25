#include "common.h"

const int CENNIK[] = { KOSZT_LEKKA, KOSZT_CIEZKA, KOSZT_JAZDA, KOSZT_ROBOTNICY };
const int CZASY[]  = { CZAS_LEKKA,  CZAS_CIEZKA,  CZAS_JAZDA,  CZAS_ROBOTNICY };

struct GameState {
    struct Player players[2];
};

int main()
{   
    key_t key = ftok(".", 'G');
    shmid = shmget(key, sizeof(struct GameState), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("Niepowodzenie tworzenia pamięci współdzielonej.");
        exit(1);
    }
    printf("Pamięć współdzielona pomyślnie utworzona z ID: %d\n", shmid);
    struct GameState *gameState = (struct GameState *)shmat(shmid, NULL, 0);
    if (gameState == (void *)-1) {
        perror("Niepowodzenie dołączenia pamięci współdzielonej.");
        exit(1);
    }
    gameState->players[0].resources = 300;
    gameState->players[1].resources = 300;
    for (int i = 0; i < 4; i++) {
        gameState->players[0].troops[i] = 0;
        gameState->players[1].troops[i] = 0;
    }

    key_t queueKey = ftok(".", 'Q');
    msgid = msgget(queueKey, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("Niepowodzenie tworzenia kolejki komunikatów.");
        exit(1);
    }
    printf("Kolejka komunikatów pomyślnie utworzona z ID: %d\n", msgid);

    key_t semKey = ftok(".", 'S');
    semid = semget(semKey, 1, 0666 | IPC_CREAT);
    if (semid == -1) {
        perror("Niepowodzenie tworzenia semafora.");
        exit(1);
    }

    semctl(semid, 0, SETVAL, 1);

    pid = fork();
    signal(SIGINT,zakoncz);
    switch (pid) {
        case -1:
            perror("Niepowodzenie tworzenia procesu potomnego.");
            exit(1);
        case 0:
            while (1) {
            struct GameMessage message;
                if (msgrcv(msgid, &message, sizeof(message.data), KANAL_SERWER, 0) == -1) {
                    perror("Niepowodzenie odbierania komunikatu.");
                    exit(1);
                }
                printf("Odebrano komunikat! Typ: %ld, Akcja: %d\n", message.mtype, message.data.action);
                int id_nadawcy = message.data.player_id;
                int id_wroga = (id_nadawcy == 0) ? 1 : 0;
                long kanal_odbiorcy = (id_nadawcy == 0) ? KANAL_GRACZ_1 : KANAL_GRACZ_2;
                switch (message.data.action) {
                    case ZLECENIE_STAN:
                        printf("Logika: Przetwarzanie zlecenia stanu zasobów.\n");
                        struct GameMessage response;
                        response.mtype = kanal_odbiorcy;
                        response.data.resources = gameState->players[id_nadawcy].resources;
                        if (msgsnd(msgid, &response, sizeof(response.data), 0) == -1) {
                            perror("Niepowodzenie wysyłania komunikatu.");
                            exit(1);
                        }
                        printf("Wysłano odpowiedź z zasobami gracza 0: %d\n", response.data.resources);
                        break;
                    case ZLECENIE_PRODUKCJA:
                        printf("Logika: Przetwarzanie zlecenia produkcji jednostek.\n");
                        blokuj(semid);
                        if (message.data.troop[JEDN_LEKKA] * 100 > gameState->players[id_nadawcy].resources) {
                            printf("Logika: Niewystarczające zasoby do produkcji lekkiej piechoty.\n");
                        }
                        else {
                        gameState->players[id_nadawcy].troops[JEDN_LEKKA] += message.data.troop[JEDN_LEKKA];
                        gameState->players[id_nadawcy].resources -= message.data.troop[JEDN_LEKKA] * 100;
                        }
                        odblokuj(semid);
                        struct GameMessage prod_response;
                        prod_response.mtype = kanal_odbiorcy;
                        prod_response.data.resources = gameState->players[id_nadawcy].resources;
                        if (msgsnd(msgid, &prod_response, sizeof(prod_response.data), 0) == -1) {
                            perror("Niepowodzenie wysyłania komunikatu.");
                            exit(1);
                        }
                        printf("Wysłano odpowiedź na produkcję. %d\n", prod_response.data.resources);
                        break;
                    case ZLECENIE_ATAK:
                        printf("Logika: Przetwarzanie zlecenia ataku.\n");
                        blokuj(semid);
                        if (message.data.troop[JEDN_LEKKA] > gameState->players[id_nadawcy].troops[JEDN_LEKKA]) {
                            printf("Logika: Niewystarczająca liczba jednostek do ataku.\n");
                        } else {
                            int sila_ataku = message.data.troop[JEDN_LEKKA];
                            int sila_obrony = gameState->players[id_wroga].troops[JEDN_LEKKA];
                            gameState->players[id_nadawcy].troops[JEDN_LEKKA] -= sila_ataku;
                            printf("Logika: Wymarsz %d jednostek. W bazie zostało %d jednostek.\n", sila_ataku, gameState->players[id_nadawcy].troops[JEDN_LEKKA]);

                            if(sila_ataku > sila_obrony) {
                                gameState->players[id_wroga].troops[JEDN_LEKKA] = 0;
                                int ocaleni = sila_ataku - sila_obrony;
                                gameState->players[id_nadawcy].troops[JEDN_LEKKA] += ocaleni;
                                printf("Logika: Zwycięstwo! Powróciło chwałą %d żołnierzy.\n", ocaleni);
                            } else if (sila_ataku < sila_obrony) {
                               gameState->players[id_wroga].troops[JEDN_LEKKA] -= sila_ataku;
                               printf("Logika: Porażka! Stracono wszystkich atakujących.\n");
                            }
                            else {
                                gameState->players[id_wroga].troops[JEDN_LEKKA] = 0;
                                printf("Logika: Remis! Obie strony straciły wszystkich żołnierzy.\n");
                            }
                        odblokuj(semid);
                        }
                        struct GameMessage attack_response;
                        attack_response.mtype = kanal_odbiorcy;
                        attack_response.data.troop[JEDN_LEKKA] = gameState->players[id_nadawcy].troops[JEDN_LEKKA];
                        if (msgsnd(msgid, &attack_response, sizeof(attack_response.data), 0) == -1) {
                            perror("Niepowodzenie wysyłania komunikatu.");
                            exit(1);
                        }
                        printf("Wysłano odpowiedź na atak.\n");
                        break;
                    default:
                        struct GameMessage error_response;
                        error_response.mtype = kanal_odbiorcy;
                        printf("Logika: Nieznane zlecenie.\n");
                        msgsnd(msgid, &error_response, sizeof(error_response.data), 0);
                }
            }
            exit(0);
        default:
            while (1)   {
                sleep(1);
                blokuj(semid);
                gameState->players[0].resources += 50;
                gameState->players[1].resources += 50;
                odblokuj(semid);
            }
    }
}