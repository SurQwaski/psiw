#include "common.h"

const int CENNIK[] = { KOSZT_LEKKA, KOSZT_CIEZKA, KOSZT_JAZDA, KOSZT_ROBOTNICY };
const int CZASY[]  = { CZAS_LEKKA,  CZAS_CIEZKA,  CZAS_JAZDA,  CZAS_ROBOTNICY };
const float ATAKI[]  = { ATAK_LEKKA,  ATAK_CIEZKA,  ATAK_JAZDA,  ATAK_ROBOTNICY };
const float OBRONY[] = { OBRONA_LEKKA, OBRONA_CIEZKA, OBRONA_JAZDA, OBRONA_ROBOTNICY };

struct GameState {
    struct Player players[2];
    struct Attack current_attack;
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

    // Inicjalizacja stanu gry //
   for (int i = 0; i < 2; i++) {
        gameState->players[i].resources = 300;
        gameState->players[i].current_production.active = 0;
        gameState->players[i].current_production.unit_type = -1;
        gameState->players[i].current_production.pending_count = 0;
        gameState->players[i].current_production.time_left = 0;
        gameState->players[i].points = 0;
    }
    for (int i = 0; i < 4; i++) {
        gameState->players[0].troops[i] = 0;
        gameState->players[1].troops[i] = 0;
    }

    gameState->current_attack.active = 0; 
    gameState->current_attack.time_left = 0;

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
            break;
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
                        response.data.action = ZLECENIE_STAN;
                        response.data.resources = gameState->players[id_nadawcy].resources;

                        if (msgsnd(msgid, &response, sizeof(response.data), 0) == -1) {
                            perror("Niepowodzenie wysyłania komunikatu.");
                            exit(1);
                        }

                        printf("Wysłano odpowiedź z zasobami gracza 0: %d\n", response.data.resources);
                        break;
                    case ZLECENIE_PRODUKCJA:
                        if (gameState->players[id_nadawcy].current_production.active) {
                            printf("Logika: Gracz już produkuje jednostki.\n");

                            struct GameMessage busy_response;
                            busy_response.mtype = kanal_odbiorcy;
                            busy_response.data.action = PRODUKCJA_TRWA;
                            msgsnd(msgid, &busy_response, sizeof(busy_response.data), 0);

                            break;
                        }
                        int unit_type = -1;
                        int ile = 0;
                        for (int i = 0; i < 4; i++) {
                            if (message.data.troop[i] > 0) {
                                unit_type = i;
                                ile = message.data.troop[i];
                                break;
                            }
                        }
                        if (unit_type == -1) {
                            printf("Logika: Nieprawidłowy typ jednostki do produkcji.\n");

                            struct GameMessage invalid_response;
                            invalid_response.mtype = kanal_odbiorcy;
                            invalid_response.data.action = NIEPRAWIDLOWY_TYP_JEDNOSTKI;
                            msgsnd(msgid, &invalid_response, sizeof(invalid_response.data), 0);

                            break;
                        }
                        int koszt = ile * CENNIK[unit_type];
                        if (koszt > gameState->players[id_nadawcy].resources) {
                            printf("Logika: Niewystarczające zasoby do rozpoczęcia produkcji.\n");

                            struct GameMessage no_resources_response;
                            no_resources_response.mtype = kanal_odbiorcy;
                            no_resources_response.data.action = INFORMACJA_BRAKU_SUROWCÓW;
                            msgsnd(msgid, &no_resources_response, sizeof(no_resources_response.data), 0);

                            break;
                        }
                        printf("Logika: Przetwarzanie zlecenia produkcji jednostek.\n");
                        blokuj(semid);
                        gameState->players[id_nadawcy].resources -= message.data.troop[unit_type] * CENNIK[unit_type];
                        gameState->players[id_nadawcy].current_production.active = 1;
                        gameState->players[id_nadawcy].current_production.unit_type = unit_type;
                        gameState->players[id_nadawcy].current_production.pending_count = ile;
                        gameState->players[id_nadawcy].current_production.time_left = CZASY[unit_type];
                        odblokuj(semid);

                        struct GameMessage prod_response;
                        prod_response.mtype = kanal_odbiorcy;
                        prod_response.data.resources = gameState->players[id_nadawcy].resources;

                        if (msgsnd(msgid, &prod_response, sizeof(prod_response.data), 0) == -1) {
                            perror("Niepowodzenie wysyłania komunikatu.");
                        }
                        printf("Wysłano odpowiedź na produkcję. %d\n", prod_response.data.resources);
                        break;
                    case ZLECENIE_ATAK:
                        printf("Logika: Przetwarzanie zlecenia ataku.\n");
                        blokuj(semid); 
                        if(gameState->current_attack.active) {
                            printf("Logika: Już trwa atak. Nie można rozpocząć nowego.\n");

                            struct GameMessage attack_busy_response;
                            attack_busy_response.mtype = kanal_odbiorcy;
                            attack_busy_response.data.action = ATAK_W_TOKU;
                            msgsnd(msgid, &attack_busy_response, sizeof(attack_busy_response.data), 0);
                            odblokuj(semid);
                            break;
                        }
                        int attacker = id_nadawcy;
                        int defender = id_wroga;

                        for (int i = 0; i < 4; i++) {
                            if (message.data.troop[i] > gameState->players[attacker].troops[i]) {
                                printf("Logika: Nie można wysłać więcej jednostek niż posiadamy.\n");

                                struct GameMessage invalid_attack_response;
                                invalid_attack_response.mtype = kanal_odbiorcy;
                                invalid_attack_response.data.action = NIEPRAWIDLOWY_ZLECENIE_ATAK;
                                msgsnd(msgid, &invalid_attack_response, sizeof(invalid_attack_response.data), 0);
                                odblokuj(semid);
                                break;
                            }
                        }
                        for (int i = 0; i < 4; i++) {
                            gameState->players[attacker].troops[i] -= message.data.troop[i];
                            gameState->current_attack.troops[i] = message.data.troop[i];
                        }
                        gameState->current_attack.active = 1;
                        gameState->current_attack.attacker = attacker;
                        gameState->current_attack.defender = defender;
                        gameState->current_attack.time_left = 5;
                        odblokuj(semid);

                        struct GameMessage attack_response;
                        attack_response.mtype = kanal_odbiorcy;
                        attack_response.data.action = ZLECENIE_ATAK;
                        msgsnd(msgid, &attack_response, sizeof(attack_response.data), 0);
                        printf("Rozpoczęto atak gracza %d na gracza %d.\n", attacker, defender);
                        break;
                        default:
                            struct GameMessage error_response;
                            error_response.mtype = kanal_odbiorcy;
                            error_response.data.action = BLAD_ZLECENIA;
                            printf("Logika: Nieznane zlecenie.\n");
                            msgsnd(msgid, &error_response, sizeof(error_response.data), 0);
                            break;
                }
            }
            break;
        default:{
            while (1) {
                sleep(1);
                blokuj(semid);

                gameState->players[0].resources += 50 + 5 * gameState->players[0].troops[JEDN_ROBOTNICY];
                gameState->players[1].resources += 50 + 5 * gameState->players[1].troops[JEDN_ROBOTNICY];

                for (int p = 0; p < 2; p++){
                    struct GameMessage currencys;
                    long kanal_odbiorcy = (p == 0) ? KANAL_GRACZ_1 : KANAL_GRACZ_2;
                    currencys.mtype = kanal_odbiorcy;
                    currencys.data.action = ZLECENIE_STAN;
                    currencys.data.resources = gameState->players[p].resources;
                    msgsnd(msgid, &currencys, sizeof(currencys.data), 0);
                }

            for(int p = 0; p < 2; p++){
                if (gameState->players[p].current_production.active) {
                    gameState->players[p].current_production.time_left -= 1;
                    if (gameState->players[p].current_production.time_left <= 0) {
                        int typ = gameState->players[p].current_production.unit_type;
                        gameState->players[p].troops[typ] += 1;
                        gameState->players[p].current_production.pending_count -= 1;

                        struct GameMessage production_complete;
                        long kanal_odbiorcy = (p == 0) ? KANAL_GRACZ_1 : KANAL_GRACZ_2;
                        production_complete.mtype = kanal_odbiorcy;
                        production_complete.data.action = ZLECENIE_PRODUKCJA;
                        production_complete.data.resources = gameState->players[p].resources;

                        for(int i = 0; i < 4; i++){
                            production_complete.data.troop[i] = gameState->players[p].troops[i];
                        }

                        msgsnd(msgid, &production_complete, sizeof(production_complete.data), 0);
                        printf("Gracz %d wyprodukował jednostkę typu %d. Pozostało do wyprodukowania: %d\n", p, typ, gameState->players[p].current_production.pending_count);

                        if (gameState->players[p].current_production.pending_count > 0) {
                           gameState->players[p].current_production.time_left = CZASY[typ];
                        } else {
                            gameState->players[p].current_production.active = 0;
                        }
                    }
                }
            }
            if (gameState->current_attack.active) {
                gameState->current_attack.time_left -= 1;
                if(gameState->current_attack.time_left > 0) {
                    continue;
                }
                int attacker = gameState->current_attack.attacker;
                int defender = gameState->current_attack.defender;
                double SA = 0;
                double SB = 0;

                SA = gameState->current_attack.troops[JEDN_LEKKA] * ATAKI[JEDN_LEKKA] +
                        gameState->current_attack.troops[JEDN_CIEZKA] * ATAKI[JEDN_CIEZKA] +
                        gameState->current_attack.troops[JEDN_JAZDA] * ATAKI[JEDN_JAZDA] +
                        gameState->current_attack.troops[JEDN_ROBOTNICY] * ATAKI[JEDN_ROBOTNICY];
                
                SB = gameState->players[defender].troops[JEDN_LEKKA] * OBRONY[JEDN_LEKKA] +
                        gameState->players[defender].troops[JEDN_CIEZKA] * OBRONY[JEDN_CIEZKA] +
                        gameState->players[defender].troops[JEDN_JAZDA] * OBRONY[JEDN_JAZDA] +
                        gameState->players[defender].troops[JEDN_ROBOTNICY] * OBRONY[JEDN_ROBOTNICY];

                int lost_defender_troops[4] = {0};

                if (SA - SB > 0){
                    lost_defender_troops[JEDN_LEKKA] = gameState->players[defender].troops[JEDN_LEKKA];
                    lost_defender_troops[JEDN_CIEZKA] = gameState->players[defender].troops[JEDN_CIEZKA];
                    lost_defender_troops[JEDN_JAZDA] = gameState->players[defender].troops[JEDN_JAZDA];
                    lost_defender_troops[JEDN_ROBOTNICY] = gameState->players[defender].troops[JEDN_ROBOTNICY];
                    
                }
                else{
                    if(SB > 0){
                        double ratio = SA / SB;
                        lost_defender_troops[JEDN_LEKKA] = (int)(gameState->players[defender].troops[JEDN_LEKKA] * ratio);
                        lost_defender_troops[JEDN_CIEZKA] = (int)(gameState->players[defender].troops[JEDN_CIEZKA] * ratio);
                        lost_defender_troops[JEDN_JAZDA] = (int)(gameState->players[defender].troops[JEDN_JAZDA] * ratio);
                        lost_defender_troops[JEDN_ROBOTNICY] = (int)(gameState->players[defender].troops[JEDN_ROBOTNICY] * ratio);
                    }
                }
                double SA2 = 0;
                double SB2 = 0;

                SA2 =   gameState->players[defender].troops[JEDN_LEKKA] * ATAKI[JEDN_LEKKA] +
                        gameState->players[defender].troops[JEDN_CIEZKA] * ATAKI[JEDN_CIEZKA] +
                        gameState->players[defender].troops[JEDN_JAZDA] * ATAKI[JEDN_JAZDA] +
                        gameState->players[defender].troops[JEDN_ROBOTNICY] * ATAKI[JEDN_ROBOTNICY];

                SB2 = gameState->current_attack.troops[JEDN_LEKKA] * OBRONY[JEDN_LEKKA] +
                    gameState->current_attack.troops[JEDN_CIEZKA] * OBRONY[JEDN_CIEZKA] +
                    gameState->current_attack.troops[JEDN_JAZDA] * OBRONY[JEDN_JAZDA] +
                    gameState->current_attack.troops[JEDN_ROBOTNICY] * OBRONY[JEDN_ROBOTNICY];

                int lost_attacker_troops[4] = {0};

                if (SA2 - SB2 > 0){
                    lost_attacker_troops[JEDN_LEKKA] = gameState->current_attack.troops[JEDN_LEKKA];
                    lost_attacker_troops[JEDN_CIEZKA] = gameState->current_attack.troops[JEDN_CIEZKA];
                    lost_attacker_troops[JEDN_JAZDA] = gameState->current_attack.troops[JEDN_JAZDA];
                    lost_attacker_troops[JEDN_ROBOTNICY] = gameState->current_attack.troops[JEDN_ROBOTNICY];
                }
                else{
                    if(SB2 > 0){
                        double ratio2 = SA2 / SB2;
                        lost_attacker_troops[JEDN_LEKKA] = (int)(gameState->current_attack.troops[JEDN_LEKKA] * ratio2);
                        lost_attacker_troops[JEDN_CIEZKA] = (int)(gameState->current_attack.troops[JEDN_CIEZKA] * ratio2);
                        lost_attacker_troops[JEDN_JAZDA] = (int)(gameState->current_attack.troops[JEDN_JAZDA] * ratio2);
                        lost_attacker_troops[JEDN_ROBOTNICY] = (int)(gameState->current_attack.troops[JEDN_ROBOTNICY] * ratio2);
                    }
                }

                for(int i = 0; i < 4; i++){
                    gameState->players[defender].troops[i] -= lost_defender_troops[i];
                    gameState->current_attack.troops[i] -= lost_attacker_troops[i];
                    gameState->players[attacker].troops[i] += gameState->current_attack.troops[i];
                }

                if(SA > SB){
                    gameState->players[attacker].points += 1;
                    printf("Gracz %d wygrał atak i zdobywa 1 punkt! Ma ich teraz %d\n", attacker, gameState->players[attacker].points);
                }

                if(gameState->players[attacker].points >= 5){
                    printf("Gracz %d wygrał grę z wynikiem 5 punktów!\n", attacker);

                    struct GameMessage koniec;
                    koniec.data.action = 999;
                    koniec.data.details = attacker;

                    koniec.mtype = KANAL_GRACZ_1;
                    msgsnd(msgid, &koniec, sizeof(koniec.data), 0);

                    koniec.mtype = KANAL_GRACZ_2;
                    msgsnd(msgid, &koniec, sizeof(koniec.data), 0);

                    zakoncz(SIGINT);
                }

                struct GameMessage score_update;

                score_update.mtype = (attacker == 0) ? KANAL_GRACZ_1 : KANAL_GRACZ_2;
                score_update.data.details = gameState->players[attacker].points;
                score_update.data.action = WYNIK_WALKI;
                for(int i = 0; i < 4; i++){
                    score_update.data.troop[i] = gameState->players[attacker].troops[i];
                }
                msgsnd(msgid, &score_update, sizeof(score_update.data), 0);

                score_update.mtype = (defender == 0) ? KANAL_GRACZ_1 : KANAL_GRACZ_2; 
                msgsnd(msgid, &score_update, sizeof(score_update.data), 0);     
                
                gameState->current_attack.time_left = 0;
                gameState->current_attack.active = 0; 
                memset(gameState->current_attack.troops, 0, sizeof(gameState->current_attack.troops)); 
            }
            
            odblokuj(semid);
        }
    }
}

}