#include "common.h"

int main(int argc, char *argv[])
{
    int client_id = atoi(argv[1]);
    key_t key = ftok(".", 'Q');
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("Niepowodzenie tworzenia kolejki komunikatów.");
        exit(1);
    }
    printf("Kolejka komunikatów pomyślnie utworzona z ID: %d\n", msgid);
    long moj_kanal = (client_id == 0) ? KANAL_GRACZ_1 : KANAL_GRACZ_2;
    while (1)
    {
    struct GameMessage message;
    memset(&message, 0, sizeof(message));
    message.data.player_id = client_id;
    printf("Wybierz akcję:\n");
    printf("1. Sprawdź stan zasobów.\n");
    printf("2. Kup lekką piechotę (koszt: 100zł).\n");
    printf("3. Atakuj przeciwnika.\n");
    printf("0. Zakończ program.\n");
    int wybor;
    scanf("%d", &wybor);
    switch (wybor)
    {
        case 1:
            printf("Wybrano sprawdzenie stanu zasobów.\n");
            message.mtype = KANAL_SERWER;
            message.data.action = ZLECENIE_STAN;
            msgsnd(msgid, &message, sizeof(message.data), 0);
            printf("Wysłano komunikat.\n");
            struct GameMessage response;
            if(msgrcv(msgid, &response, sizeof(response.data), moj_kanal, 0) == -1) {
                perror("Niepowodzenie odbierania komunikatu.");
                exit(1);
            }
            printf("Otrzymano stan gry! Surowce: %d\n", response.data.resources);
            break;
        case 2:
            printf("Wybrano zakup lekkiej piechoty.\n");
            message.data.troop[JEDN_LEKKA] = 1;
            message.mtype = KANAL_SERWER;
            message.data.action = ZLECENIE_PRODUKCJA;
            msgsnd(msgid, &message, sizeof(message.data), 0);
            printf("Wysłano komunikat.\n");
            struct GameMessage production_response;
            if(msgrcv(msgid, &production_response, sizeof(production_response.data), moj_kanal, 0) == -1) {
                perror("Niepowodzenie odbierania komunikatu.");
                exit(1);
            }
            printf("Otrzymano odpowiedź na produkcję! Surowce: %d\n", production_response.data.resources);
            break;
        case 3:
            printf("Wybrano atak na przeciwnika.\n");
            printf("Ile jednostek lekkiej piechoty wysłać do ataku? ");
            int liczba_jednostek;
            scanf("%d", &liczba_jednostek);
            message.data.troop[JEDN_LEKKA] = liczba_jednostek;
            message.mtype = KANAL_SERWER;
            message.data.action = ZLECENIE_ATAK;
            msgsnd(msgid, &message, sizeof(message.data), 0);
            printf("Wysłano komunikat.\n");
            struct GameMessage attack_response;
            if(msgrcv(msgid, &attack_response, sizeof(attack_response.data), moj_kanal, 0) == -1) {
                perror("Niepowodzenie odbierania komunikatu.");
                exit(1);
            }
            printf("Otrzymano odpowiedź na atak!\n");
            break;
        case 0:
            printf("Zakończenie programu.\n");
            exit(0);
        default:
            printf("Nieprawidłowa akcja.\n");
    }
    }
}