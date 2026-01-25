#include "common.h"

struct SharedState {
    int surowce;
    int jednostki[4];
    int punkty;
};

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Użycie: %s <ID_klienta (0 lub 1)>\n", argv[0]);
        exit(1);
    }

    int client_id = atoi(argv[1]);
    if (client_id != 0 && client_id != 1) {
        fprintf(stderr, "ID klienta musi być 0 lub 1.\n");
        exit(1);
    }

    key_t key = ftok(".", 'Q');
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }

    key_t shmkey = ftok(".", 'C'); // C = Client
    int shmid = shmget(shmkey, sizeof(struct SharedState), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("Błąd tworzenia pamięci współdzielonej.");
        exit(1);
    }

    struct SharedState *shared = shmat(shmid, NULL, 0);
    if (shared == (void*)-1) {
        perror("Błąd dołączania pamięci współdzielonej.");
        exit(1);
    }

    shared->surowce = 0;
    shared->punkty = 0;
    for (int i = 0; i < 4; i++)
        shared->jednostki[i] = 0;

    long moj_kanal = (client_id == 0) ? KANAL_GRACZ_1 : KANAL_GRACZ_2;

    pid_t listener = fork();
    if (listener == -1) {
        perror("Błąd tworzenia procesu potomnego.");
        exit(1);
    }

    if (listener == 0) {

        while (1) {
            struct GameMessage incoming;
            if (msgrcv(msgid, &incoming, sizeof(incoming.data), moj_kanal, 0) == -1) {
                perror("Błąd odbierania wiadomości.");
                exit(1);
            }

            switch (incoming.data.action) {

                case ZLECENIE_STAN:
                    shared->surowce = incoming.data.resources;
                    break;

                case ZLECENIE_PRODUKCJA:
                    shared->surowce = incoming.data.resources;
                    for (int i = 0; i < 4; i++)
                        shared->jednostki[i] = incoming.data.troop[i];
                    break;

                case WYNIK_WALKI:
                    shared->punkty = incoming.data.details;
                    for (int i = 0; i < 4; i++)
                        shared->jednostki[i] = incoming.data.troop[i];
                    printf("\n=== WYNIK WALKI ===\n");
                    printf("Punkty: %d\n", shared->punkty);
                    break;

                case PRODUKCJA_TRWA:
                    printf("\nProdukcja już trwa!\n");
                    break;

                case INFORMACJA_BRAKU_SUROWCÓW:
                    printf("\nBrak surowców!\n");
                    break;

                case NIEPRAWIDLOWY_TYP_JEDNOSTKI:
                    printf("\nNieprawidłowy typ jednostki!\n");
                    break;

                case NIEPRAWIDLOWY_ZLECENIE_ATAK:
                    printf("\nNieprawidłowy atak!\n");
                    break;

                case ATAK_W_TOKU:
                    printf("\nAtak już trwa!\n");
                    break;

                case ZLECENIE_ATAK:
                    printf("\nAtak rozpoczęty!\n");
                    break;

                case BLAD_ZLECENIA:
                    printf("\nNieznane zlecenie!\n");
                    break;

                case 999:
                    printf("\n=== KONIEC GRY ===\n");
                    printf("Wygrał gracz %d!\n", incoming.data.details);
                    exit(0);
            }
        }
    }

    while (1) {

        printf("\n=== TWÓJ STAN ===\n");
        printf("Surowce: %d\n", shared->surowce);
        printf("Jednostki: L:%d C:%d J:%d R:%d\n",
               shared->jednostki[0],
               shared->jednostki[1],
               shared->jednostki[2],
               shared->jednostki[3]);
        printf("Punkty: %d\n", shared->punkty);
        printf("=================\n");

        printf("1. Sprawdź stan\n");
        printf("2. Kup lekką piechotę\n");
        printf("3. Kup ciężką piechotę\n");
        printf("4. Kup jazdę\n");
        printf("5. Kup robotników\n");
        printf("6. Atakuj\n");
        printf("0. Wyjście\n");

        int wybor;
        scanf("%d", &wybor);

        struct GameMessage msg;
        memset(&msg, 0, sizeof(msg));
        msg.mtype = KANAL_SERWER;
        msg.data.player_id = client_id;

        switch (wybor) {
            case 1:
                msg.data.action = ZLECENIE_STAN;
                break;

            case 2:
                printf("Ile lekkiej piechoty? ");
                scanf("%d", &msg.data.troop[JEDN_LEKKA]);
                msg.data.action = ZLECENIE_PRODUKCJA;
                break;

            case 3:
                printf("Ile ciężkiej piechoty? ");
                scanf("%d", &msg.data.troop[JEDN_CIEZKA]);
                msg.data.action = ZLECENIE_PRODUKCJA;
                break;

            case 4:
                printf("Ile jazdy? ");
                scanf("%d", &msg.data.troop[JEDN_JAZDA]);
                msg.data.action = ZLECENIE_PRODUKCJA;
                break;

            case 5:
                printf("Ile robotników? ");
                scanf("%d", &msg.data.troop[JEDN_ROBOTNICY]);
                msg.data.action = ZLECENIE_PRODUKCJA;
                break;

            case 6:
                printf("Lekkiej: "); scanf("%d", &msg.data.troop[0]);
                printf("Ciężkiej: "); scanf("%d", &msg.data.troop[1]);
                printf("Jazdy: ");    scanf("%d", &msg.data.troop[2]);
                printf("Robotników: "); scanf("%d", &msg.data.troop[3]);
                msg.data.action = ZLECENIE_ATAK;
                break;

            case 0:
                printf("Zamykanie klienta...\n");
                exit(0);

            default:
                printf("Nieprawidłowa opcja.\n");
                continue;
        }

        msgsnd(msgid, &msg, sizeof(msg.data), 0);
    }
}
