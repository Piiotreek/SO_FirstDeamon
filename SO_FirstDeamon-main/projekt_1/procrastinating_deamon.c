#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

volatile sig_atomic_t signal_flag = 0; // flaga przechwyconego sygnalu
volatile sig_atomic_t malicious_mode = 0; // flaga zlosliwego trybu

// Statystyki i zmeczenie
int tasks_completed = 0; // licznik wykonanych zadan
int tasks_refused = 0; // licznik odrzuconych zadan
int files_deleted = 0; // licznik zlosliwie usunietych plikow
long bytes_processed = 0; // laczna liczba przetworzonych bajtow
int is_tired = 0; // flaga zmeczenia

// Prosta funkcja haszujaca
unsigned int hash_byte(unsigned char byte, unsigned int seed) {
    return ((byte + seed) * 2654435761U) % 256; // haszowanie z ziarnem
}

void handle_signal(int sig) {
    signal_flag = 1; // ustaw flage
}

void handle_evil_signal(int sig) {
    malicious_mode = 1; // wlacz zlosliwy tryb
}

void log_msg(char *msg) {
    FILE *log = fopen("/tmp/daemon_log.txt", "a"); // otworz plik logu
    if (log != NULL) {
        time_t t = time(NULL); // pobierz czas
        struct tm *tm = localtime(&t); // sformatuj czas
        fprintf(log, "[%02d:%02d:%02d] %s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, msg);
        fclose(log); // zamknij log
    }
}

int is_break_time() {
    time_t t = time(NULL); // pobierz aktualny czas
    struct tm *tm = localtime(&t); // zamien na strukture czasu
    if (tm->tm_hour == 16) return 1; // od 16:00 do 16:59 jest przerwa
    return 0; // wracaj do roboty
}

int get_weekday() {
    time_t t = time(NULL); // pobierz aktualny czas
    struct tm *tm = localtime(&t); // zamien na strukture czasu
    return tm->tm_wday; // 0=niedziela, 1=poniedzialek, ..., 6=sobota
}

void save_stats() {
    FILE *stats = fopen("/tmp/daemon_stats.txt", "w"); // zapisz statystyki
    if (stats != NULL) {
        fprintf(stats, "=== Statystyki Leniwego Demona ===\n");
        fprintf(stats, "Zadan wykonanych: %d\n", tasks_completed);
        fprintf(stats, "Zadan odrzuconych: %d\n", tasks_refused);
        fprintf(stats, "Plikow usunietych (zlosliwie): %d\n", files_deleted);
        fprintf(stats, "Bajtow przetworzonych: %ld\n", bytes_processed);
        fprintf(stats, "Stan: %s\n", is_tired ? "ZMECZONY" : "Wypoczety");
        fclose(stats);
    }
}

void make_daemon() {
    pid_t pid = fork(); // utworz proces potomny
    if (pid < 0) exit(1); // fork sie nie udal
    if (pid > 0) exit(0); // zakoncz proces macierzysty
    setsid(); // utworz nowa sesje
    close(STDIN_FILENO); // zamknij wejscie
    close(STDOUT_FILENO); // zamknij wyjscie
    close(STDERR_FILENO); // zamknij bledy
}

long get_size(char *path) {
    struct stat st; // informacje o pliku
    if (stat(path, &st) == 0) return st.st_size; // zwroc liczbe bajtow
    return 0; // domyslnie zero
}

void process_file(int mode, int delay, char *path, char *dest) {
    int weekday = get_weekday();

    // Sprawdzenie weekendu
    if (weekday == 0 || weekday == 6) {
        log_msg("Weekend! Nie pracuje w weekendy, wracaj w poniedzialek!");
        tasks_refused++;
        save_stats();
        return;
    }

    // Sprawdzenie zmeczenia
    if (is_tired) {
        log_msg("Jestem zmeczony... potrzebuje 5 minut przerwy...");
        sleep(300); // 5 minut przerwy
        is_tired = 0; // zresetuj po odpoczynku
        log_msg("Ok, juz troche odpoczalem.");
    }

    // Poniedzialkowa niechec do zycia
    if (weekday == 1) {
        log_msg("Ugh, poniedzialek... najgorszy dzien tygodnia...");
        sleep(5); // dodatkowe opoznienie za kare za poniedzialek
        log_msg("No dobra, moge sie za to zabrac...");
    }

    if (is_break_time()) {
        log_msg("Eee nie, teraz mam przerwe kawowa (16:00-17:00)!");
        tasks_refused++;
        save_stats();
        return; // ignore task completely
    }

    if (delay > 0) {
        log_msg("*Yawn*...");
        sleep(delay); // najpierw sie zdrzemnij
    }

    if (get_size(path) > 500) {
        log_msg("Za duzy plik, nie chce mi sie, ide spac...");
        sleep(10); // opoznij wykonanie
        log_msg("Ok, moge zaczynac.");
    }

    if (get_size(path) > 2000) {
        log_msg("Co to jest? Nieeee za dlugie, ide spac");
        sleep(120); // spij 2 minuty
        log_msg("DOBRA DOBRA usiade do tego, ugh...");
    }

    // Losowe akcje!
    int random_event = rand() % 100;
    if (random_event < 5) { // 5% szans
        log_msg("Ups, myszka mi uciekla! Gdzie ona jest?");
        sleep(30); // opoznienie 30 sekund
        log_msg("Znalazlem! Dobra, wracam do pracy...");
    } else if (random_event < 15) { // 10% szans (5-15)
        log_msg("Hmm, musze sprawdzic co nowego na reddicie...");
        sleep(60); // opoznienie 60 sekund
        log_msg("Czy czegos zapomnialem?");
        log_msg("...");
        log_msg("O szlag! Moja praca! No przeciez!");
    } else if (random_event < 25) { // 10% szans (15-25)
        log_msg("Nie! Nie chce mi sie, nie zrobie tego!");
        tasks_refused++;
        save_stats();
        return; // odrzuc zadanie calkowicie
    }

    char final_dest[256];
    if (signal_flag == 1) {
        strcpy(final_dest, "/tmp/signal_secret_file.txt"); // inny katalog docelowy
        log_msg("Opa! Jakis sygnal? Inna sciezka.");
        signal_flag = 0; // zresetuj flage
    } else {
        strcpy(final_dest, dest); // zwykly cel
    }

    FILE *in = fopen(path, "rb"); // otworz do odczytu binarnego
    FILE *out = fopen(final_dest, "wb"); // otworz do zapisu binarnego
    if (!in || !out) {
        log_msg("No i pieknie, plik sie zbuntowal i nic z tego nie bedzie.");
        if (in) fclose(in);
        return; // zatrzymaj przetwarzanie
    }

    unsigned int seed = (unsigned int)time(NULL); // ziarno dla hasha
    int c;
    int evil = malicious_mode || (rand() % 2 == 1); // losowa lub wymuszona zlosliwosc

    if (mode == 1) { // szyfrowanie hashem
        if (evil) {
            log_msg("Hehehe, bede zlosliwy dzisiaj >:)");
            fprintf(out, "%u\n", seed); // zapisz ziarno jako tekst
            while ((c = fgetc(in)) != EOF) {
                unsigned char hashed = (unsigned char)hash_byte((unsigned char)c, seed);
                fprintf(out, "%u ", hashed); // zapis tekstowy - plik niepotrzebnie puchnie!
            }
        } else {
            fwrite(&seed, sizeof(unsigned int), 1, out); // zapisz ziarno binarnie
            while ((c = fgetc(in)) != EOF) {
                unsigned char hashed = (unsigned char)hash_byte((unsigned char)c, seed);
                fputc(hashed, out); // zapisz jako bajt - normalny rozmiar
            }
        }
    } else if (mode == 2) { // odszyfrowywanie hasha
        // sproboj wykryc format - tekstowy albo binarny
        long pos = ftell(in);
        char test[10];
        fgets(test, 10, in);
        fseek(in, pos, SEEK_SET); // cofnij wskaznik pliku

        if (test[0] >= '0' && test[0] <= '9') { // format tekstowy
            log_msg("Ojoj, to byl zlosliwy hash, dekoduje...");
            fscanf(in, "%u\n", &seed); // odczytaj ziarno z tekstu
            unsigned int hashed;
            while (fscanf(in, "%u ", &hashed) == 1) {
                // odwroc hasz metoda brute force
                for (int i = 0; i < 256; i++) {
                    if (hash_byte((unsigned char)i, seed) == (unsigned char)hashed) {
                        fputc(i, out); // zapisz oryginalna wartosc
                        break;
                    }
                }
            }
        } else { // format binarny
            fread(&seed, sizeof(unsigned int), 1, in); // odczytaj ziarno binarnie
            while ((c = fgetc(in)) != EOF) {
                // odwroc hasz metoda brute force
                for (int i = 0; i < 256; i++) {
                    if (hash_byte((unsigned char)i, seed) == (unsigned char)c) {
                        fputc(i, out); // zapisz oryginalna wartosc
                        break;
                    }
                }
            }
        }
    }

    fclose(in); // zamknij plik zrodlowy
    fclose(out); // zamknij plik docelowy

    // Aktualizacja statystyk
    long file_size = get_size(path);
    bytes_processed += file_size;
    tasks_completed++;

    // Sprawdzenie zmeczenia po 10 zadaniach
    if (tasks_completed % 10 == 0) {
        is_tired = 1; // ustaw flage zmeczenia
        log_msg("Uff, 10 zadan juz zrobilem. Jestem zmeczony...");
    }

    log_msg("Tak tak, zrobilem to.");
    save_stats(); // zapisz po kazdym zadaniu

    if (mode == 1 && (rand() % 2 == 1)) { // losowe 50% szans
        remove(path); // usun oryginalny plik
        files_deleted++; // zlicz usuniecie
        save_stats(); // odswiez statystyki
        log_msg("Ale fajny plik. Pozwol ze go usune :>.");
    }

    malicious_mode = 0; // zresetuj zlosliwy tryb po uzyciu
}

int main() {
    make_daemon(); // zamien proces w demona
    signal(SIGUSR1, handle_signal); // nasluchuj sygnalu
    signal(SIGUSR2, handle_evil_signal); // wlacznik zlosliwego trybu
    srand(time(NULL)); // ustaw ziarno losowosci
    log_msg("Demon wstal z lozka i jakos zaczal zmiane.");

    while (1) {
        FILE *task = fopen("/tmp/task.txt", "r"); // sprawdz, czy sa nowe zadania
        if (task != NULL) {
            int mode, delay;
            char path[256], dest[256];
            if (fscanf(task, "%d\n%d\n%255s\n%255s", &mode, &delay, path, dest) == 4) {
                fclose(task); // zamknij plik zadania
                remove("/tmp/task.txt"); // usun, zeby nie zapetlic przetwarzania
                
                if (mode == 3) {
                    int random_nah = rand() % 10;
                    if (random_nah < 1) {
                        log_msg("Myslisz, ze tak latwo mnie stad wyrzucisz? Niedoczekanie, jeszcze tu posiedze!");
                        tasks_refused++;
                        save_stats();
                        continue;
                    } else {
                        log_msg("Koniec pracy na dzisiaj? W koncu! Ide do domu sie wyspac.");
                        exit(0); // zakoncz dzialanie demona
                    }
                }
                process_file(mode, delay, path, dest); // odwal cala robote
            } else {
                fclose(task); // zamknij plik zadania
                remove("/tmp/task.txt"); // usun plik o zlym formacie
            }
        }
        sleep(2); // chwila leniwej przerwy miedzy obiegami
    }
    return 0; // zakoncz program
}