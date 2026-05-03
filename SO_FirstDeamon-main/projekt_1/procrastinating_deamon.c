#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

volatile sig_atomic_t signal_flag = 0; // catch signal
volatile sig_atomic_t malicious_mode = 0; // evil mode flag

// Statistics and fatigue
int tasks_completed = 0; // task counter
int tasks_refused = 0; // refused tasks
int files_deleted = 0; // maliciously deleted
long bytes_processed = 0; // total bytes
int is_tired = 0; // fatigue flag

// Simple hash function
unsigned int hash_byte(unsigned char byte, unsigned int seed) {
    return ((byte + seed) * 2654435761U) % 256; // hash with seed
}

void handle_signal(int sig) {
    signal_flag = 1; // set flag
}

void handle_evil_signal(int sig) {
    malicious_mode = 1; // activate evil mode
}

void log_msg(char *msg) {
    FILE *log = fopen("/tmp/daemon_log.txt", "a"); // open log
    if (log != NULL) {
        time_t t = time(NULL); // get time
        struct tm *tm = localtime(&t); // format time
        fprintf(log, "[%02d:%02d:%02d] %s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, msg);
        fclose(log); // close log
    }
}

int is_break_time() {
    time_t t = time(NULL); // get current time
    struct tm *tm = localtime(&t); // convert to struct
    if (tm->tm_hour == 16) return 1; // 16:00 to 16:59 is break
    return 0; // go to work
}

int get_weekday() {
    time_t t = time(NULL); // get current time
    struct tm *tm = localtime(&t); // convert to struct
    return tm->tm_wday; // 0=Sunday, 1=Monday, ..., 6=Saturday
}

void save_stats() {
    FILE *stats = fopen("/tmp/daemon_stats.txt", "w"); // save stats
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
    pid_t pid = fork(); // create child
    if (pid < 0) exit(1); // fork failed
    if (pid > 0) exit(0); // kill parent
    setsid(); // create new session
    close(STDIN_FILENO); // close input
    close(STDOUT_FILENO); // close output
    close(STDERR_FILENO); // close errors
}

long get_size(char *path) {
    struct stat st; // file info
    if (stat(path, &st) == 0) return st.st_size; // return bytes
    return 0; // default zero
}

void process_file(int mode, int delay, char *path, char *dest) {
    int weekday = get_weekday();

    // Weekend check
    if (weekday == 0 || weekday == 6) {
        log_msg("Weekend! Nie pracuje w weekendy, wracaj w poniedzialek!");
        tasks_refused++;
        save_stats();
        return;
    }

    // Fatigue check
    if (is_tired) {
        log_msg("Jestem zmeczony... potrzebuje 5 minut przerwy...");
        sleep(300); // 5 minutes
        is_tired = 0; // reset after rest
        log_msg("Ok, juz troche odpoczalem.");
    }

    // Monday blues
    if (weekday == 1) {
        log_msg("Ugh, poniedzialek... najgorszy dzien tygodnia...");
        sleep(5); // extra Monday delay
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
        sleep(delay); // sleep first
    }

    if (get_size(path) > 500) {
        log_msg("Za duzy plik, nie chce mi sie, ide spac...");
        sleep(10); // delay execution
        log_msg("Ok, moge zaczynac.");
    }

    if (get_size(path) > 2000) {
        log_msg("Co to jest? Nieeee za dlugie, ide spac");
        sleep(120); // sleep 2 minutes
        log_msg("DOBRA DOBRA usiade do tego, ugh...");
    }

    // Random events!
    int random_event = rand() % 100;
    if (random_event < 5) { // 5% chance
        log_msg("Ups, myszka mi uciekla! Gdzie ona jest?");
        sleep(30); // 30 seconds delay
        log_msg("Znalazlem! Dobra, wracam do pracy...");
    } else if (random_event < 15) { // 10% chance (5-15)
        log_msg("Hmm, musze sprawdzic co nowego na reddicie...");
        sleep(60); // 60 seconds delay
        log_msg("Czy czegos zapomnialem?");
        log_msg("...");
        log_msg("O szlag! Moja praca! No przeciez!");
    } else if (random_event < 25) { // 10% chance (15-25)
        log_msg("Nie! Nie chce mi sie, nie zrobie tego!");
        tasks_refused++;
        save_stats();
        return; // refuse completely
    }

    char final_dest[256];
    if (signal_flag == 1) {
        strcpy(final_dest, "/tmp/signal_secret_file.txt"); // diff dir
        log_msg("Opa! Jakis sygnal? Inna sciezka.");
        signal_flag = 0; // reset flag
    } else {
        strcpy(final_dest, dest); // normal dest
    }

    FILE *in = fopen(path, "rb"); // read binary
    FILE *out = fopen(final_dest, "wb"); // write binary
    if (!in || !out) {
        log_msg("File error.");
        if (in) fclose(in);
        return; // stop
    }

    unsigned int seed = (unsigned int)time(NULL); // seed for hash
    int c;
    int evil = malicious_mode || (rand() % 2 == 1); // random or forced evil

    if (mode == 1) { // hash encryption
        if (evil) {
            log_msg("Hehehe, bede zlosliwy dzisiaj >:)");
            fprintf(out, "%u\n", seed); // save seed as text
            while ((c = fgetc(in)) != EOF) {
                unsigned char hashed = (unsigned char)hash_byte((unsigned char)c, seed);
                fprintf(out, "%u ", hashed); // write as TEXT - file bloat!
            }
        } else {
            fwrite(&seed, sizeof(unsigned int), 1, out); // save seed binary
            while ((c = fgetc(in)) != EOF) {
                unsigned char hashed = (unsigned char)hash_byte((unsigned char)c, seed);
                fputc(hashed, out); // write as byte - normal size
            }
        }
    } else if (mode == 2) { // hash decryption
        // try to detect format - text or binary
        long pos = ftell(in);
        char test[10];
        fgets(test, 10, in);
        fseek(in, pos, SEEK_SET); // rewind

        if (test[0] >= '0' && test[0] <= '9') { // text format
            log_msg("Ojoj, to byl zlosliwy hash, dekoduje...");
            fscanf(in, "%u\n", &seed); // read seed from text
            unsigned int hashed;
            while (fscanf(in, "%u ", &hashed) == 1) {
                // reverse hash by brute force
                for (int i = 0; i < 256; i++) {
                    if (hash_byte((unsigned char)i, seed) == (unsigned char)hashed) {
                        fputc(i, out); // write original
                        break;
                    }
                }
            }
        } else { // binary format
            fread(&seed, sizeof(unsigned int), 1, in); // read seed binary
            while ((c = fgetc(in)) != EOF) {
                // reverse hash by brute force
                for (int i = 0; i < 256; i++) {
                    if (hash_byte((unsigned char)i, seed) == (unsigned char)c) {
                        fputc(i, out); // write original
                        break;
                    }
                }
            }
        }
    }

    fclose(in); // close source
    fclose(out); // close dest

    // Update statistics
    long file_size = get_size(path);
    bytes_processed += file_size;
    tasks_completed++;

    // Check fatigue after 10 tasks
    if (tasks_completed % 10 == 0) {
        is_tired = 1; // set fatigue flag
        log_msg("Uff, 10 zadan juz zrobilem. Jestem zmeczony...");
    }

    log_msg("Tak tak, zrobilem to.");
    save_stats(); // save after each task

    if (mode == 1 && (rand() % 2 == 1)) { // random 50% chance
        remove(path); // delete original
        files_deleted++; // count deletion
        save_stats(); // update stats
        log_msg("Ale fajny plik. Pozwol ze go usune :>.");
    }

    malicious_mode = 0; // reset evil flag after use
}

int main() {
    make_daemon(); // turn to daemon
    signal(SIGUSR1, handle_signal); // wait for signal
    signal(SIGUSR2, handle_evil_signal); // evil mode trigger
    srand(time(NULL)); // random seed
    log_msg("Daemon started successfully.");

    while (1) {
        FILE *task = fopen("/tmp/task.txt", "r"); // look for tasks
        if (task != NULL) {
            int mode, delay;
            char path[256], dest[256];
            if (fscanf(task, "%d\n%d\n%255s\n%255s", &mode, &delay, path, dest) == 4) {
                fclose(task); // close task
                remove("/tmp/task.txt"); // delete to not loop
                
                if (mode == 3) {
                    log_msg("Terminate command received. Goodbye!");
                    exit(0); // kill daemon
                }
                process_file(mode, delay, path, dest); // do magic
            } else {
                fclose(task); // close task
                remove("/tmp/task.txt"); // delete bad format
            }
        }
        sleep(2); // rest loop
    }
    return 0; // end
}