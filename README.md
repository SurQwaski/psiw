============================================================
        PROJEKT PSiW 2025 – GRA STRATEGICZNA 1v1
============================================================

Autor: Kacper Janoszek  
Przedmiot: Programowanie Systemowe i Współbieżne  
Architektura: Klient–Serwer z IPC System V  
Mechanizmy: Kolejki komunikatów, pamięć współdzielona, semafory

------------------------------------------------------------
1. OPIS PROJEKTU
------------------------------------------------------------

Projekt implementuje dwuosobową grę strategiczną, w której gracze:

 • zbierają surowce,
 • produkują jednostki,
 • przeprowadzają ataki,
 • zdobywają punkty zwycięstwa.

Serwer przechowuje pełny stan gry i realizuje całą logikę:
 • przyrost surowców,
 • produkcja jednostek,
 • obsługa ataków,
 • wyliczanie strat,
 • przyznawanie punktów,
 • zakończenie gry.

Klient pełni rolę interfejsu użytkownika i komunikuje się z serwerem
za pomocą kolejki komunikatów System V.

------------------------------------------------------------
2. KOMPILACJA
------------------------------------------------------------

W katalogu projektu uruchom:

    make

Powstaną dwa pliki wykonywalne:

    server
    client

------------------------------------------------------------
3. URUCHOMIENIE
------------------------------------------------------------

1) Uruchom serwer:

    ./server

2) Uruchom dwóch klientów w osobnych terminalach:

    ./client 0
    ./client 1

Klient:
 • odbiera komunikaty z serwera,
 • wyświetla aktualny stan gry,
 • wysyła polecenia (produkcja, atak, stan).

------------------------------------------------------------
4. ZAWARTOŚĆ PLIKÓW
------------------------------------------------------------

server.c
    • pełna logika gry
    • obsługa produkcji i ataków
    • przyrost surowców
    • pamięć współdzielona + semafory
    • komunikacja z klientami

client.c
    • interfejs użytkownika
    • odbiór komunikatów
    • wyświetlanie stanu gry
    • wysyłanie poleceń

common.h
    • definicje struktur
    • stałe gry
    • kanały komunikacji
    • funkcje semaforów

PROTOCOL.txt
    • opis protokołu komunikacyjnego
    • format komunikatów
    • struktura pamięci współdzielonej

------------------------------------------------------------
5. ZASADY GRY
------------------------------------------------------------

 • Surowce: +50/s + 5/s za każdego robotnika
 • Produkcja jednostek trwa określony czas
 • Atak trwa 5 sekund
 • Straty liczone według algorytmu SA/SB
 • Wygrywa gracz, który zdobędzie 5 punktów

------------------------------------------------------------
6. WYMAGANIA
------------------------------------------------------------

 • Linux
 • GCC
 • IPC System V:
      - kolejki komunikatów
      - pamięć współdzielona
      - semafory

------------------------------------------------------------
7. CZYSZCZENIE
------------------------------------------------------------

    make clean

------------------------------------------------------------
8. UWAGI KOŃCOWE
------------------------------------------------------------

 • Klient nie przechowuje logiki gry.
 • Serwer działa w dwóch procesach.
 • Komunikacja jest w pełni asynchroniczna.
 • Projekt spełnia wymagania pełnej wersji.

============================================================
                KONIEC PLIKU README
============================================================
