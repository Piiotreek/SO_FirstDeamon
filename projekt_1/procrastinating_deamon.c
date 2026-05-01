#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>

volatile sig_atomic_t signal_flag = 0; // catch signal
char codes[256][256]; // huffman string codes

// Huffman tree node
struct Node {
    int ch; // character
    int freq; // frequency
    struct Node *left, *right; // children
};

void handle_signal(int sig) {
    signal_flag = 1; // set flag
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

// simple bubble sort for nodes
void sort_nodes(struct Node** nodes, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (nodes[j]->freq < nodes[j+1]->freq) {
                struct Node* temp = nodes[j]; // swap
                nodes[j] = nodes[j+1];
                nodes[j+1] = temp; // descending order
            }
        }
    }
}

// build huffman tree
struct Node* build_tree(int* freq) {
    struct Node* nodes[256]; // array of nodes
    int count = 0;
    
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            nodes[count] = malloc(sizeof(struct Node)); // new node
            nodes[count]->ch = i; // set char
            nodes[count]->freq = freq[i]; // set freq
            nodes[count]->left = NULL; // no child
            nodes[count]->right = NULL; // no child
            count++; // inc count
        }
    }
    
    if (count == 0) return NULL; // empty file
    
    while (count > 1) {
        sort_nodes(nodes, count); // sort array
        struct Node* left = nodes[count-1]; // lowest
        struct Node* right = nodes[count-2]; // second lowest
        
        struct Node* parent = malloc(sizeof(struct Node)); // make parent
        parent->ch = -1; // parent has no char
        parent->freq = left->freq + right->freq; // sum freq
        parent->left = left; // attach left
        parent->right = right; // attach right
        
        nodes[count-2] = parent; // put parent in array
        count--; // shrink array
    }
    return nodes[0]; // return root
}

// generate strings of 0s and 1s
void generate_codes(struct Node* root, char* str) {
    if (!root) return; // safety
    if (root->left == NULL && root->right == NULL) {
        strcpy(codes[root->ch], str); // save string code
        return;
    }
    char left_str[256], right_str[256];
    sprintf(left_str, "%s0", str); // add 0
    sprintf(right_str, "%s1", str); // add 1
    generate_codes(root->left, left_str); // go left
    generate_codes(root->right, right_str); // go right
}

void process_file(int mode, int delay, char *path, char *dest) {
    if (is_break_time()) {
        log_msg("Break time (16:00-17:00). I am not doing this now.");
        return; // ignore task completely
    }

    if (delay > 0) {
        log_msg("Executing later...");
        sleep(delay); // sleep first
    }

    if (get_size(path) > 500) {
        log_msg("File too big, procrastination started...");
        sleep(10); // delay execution
        log_msg("Done procrastinating.");
    }

    char final_dest[256];
    if (signal_flag == 1) {
        strcpy(final_dest, "/tmp/signal_secret_file.txt"); // diff dir
        log_msg("Signal caught! Forced different directory.");
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

    int freq[256] = {0}; // frequency array
    int c;

    if (mode == 1) { // huffman fake encryption
        while ((c = fgetc(in)) != EOF) freq[c]++; // count chars
        fwrite(freq, sizeof(int), 256, out); // save header for decrypt
        
        struct Node* root = build_tree(freq); // build tree
        memset(codes, 0, sizeof(codes)); // clear codes
        generate_codes(root, ""); // create dictionary
        
        rewind(in); // back to start
        while ((c = fgetc(in)) != EOF) {
            fputs(codes[c], out); // write string of 0s and 1s
        }
    } else if (mode == 2) { // huffman decryption
        fread(freq, sizeof(int), 256, in); // read header
        struct Node* root = build_tree(freq); // rebuild tree
        struct Node* curr = root; // start at root
        
        while ((c = fgetc(in)) != EOF) {
            if (c == '0') curr = curr->left; // move left
            else if (c == '1') curr = curr->right; // move right
            
            if (curr->left == NULL && curr->right == NULL) {
                fputc(curr->ch, out); // write original char
                curr = root; // reset to root
            }
        }
    }

    fclose(in); // close source
    fclose(out); // close dest
    log_msg("Task done.");

    if (mode == 1 && (rand() % 2 == 1)) { // random 50% chance
        remove(path); // delete original
        log_msg("Whim activated! Original file deleted.");
    }
}

int main() {
    make_daemon(); // turn to daemon
    signal(SIGUSR1, handle_signal); // wait for signal
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