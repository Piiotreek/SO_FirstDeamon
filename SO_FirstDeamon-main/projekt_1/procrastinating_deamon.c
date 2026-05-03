#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

volatile sig_atomic_t signal_flag = 0; // catch signal
volatile sig_atomic_t malicious_mode = 0; // evil mode flag

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
    if (is_break_time()) {
        log_msg("Eee nie, teraz mam przerwe kawowa (16:00-17:00)!");
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
    log_msg("Tak tak, zrobilem to.");

    if (mode == 1 && (rand() % 2 == 1)) { // random 50% chance
        remove(path); // delete original
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