#include "user/stdio.h"
#include "user/shell.h"
#include "user/syscall.h"
#include "user/dirent.h"
#include "user/stdlib.h"
#include "libc/string.h"
#include "libc/stdio.h"
#include "user/term.h"
#include "common/syscall.h" // file descriptors

#define MAX_INPUT 256
#define MAX_ARGS 10
#define MAX_HISTORY 10

char cwd[256];
char history_buffer[MAX_HISTORY][MAX_INPUT];
int history_pos = 0;
int history_count = 0;

void echo_command(char **args) {
    if (args[1]) {
        printf("%s\n", args[1]);
    }
}

void exit_command() {
    puts("Exiting shell\n");
    syscall_exit(0);
}

void history_command() {
    for (int i = 0; i < history_count; i++) {
        printf("%d: %s\n", i, history_buffer[i]);
    }
}

void ls_command(char **args) {
    char *path = args[1];
    if (path == NULL) path = ".";

    int fd = syscall_open(path, O_RDONLY);
    if (fd < 0) {
        printf("Error: Failed to open directory\n");
        return;
    }

    linux_dirent_t entries[8];
    int bytes_read = syscall_getdents(fd, entries, sizeof(entries));
    if (bytes_read < 0) {
        printf("Error: Failed to read directory\n");
        return;
    }

    for (size_t i = 0; i < bytes_read / sizeof(linux_dirent_t); i++) {
        printf("    %s\n", entries[i].d_name);
    }

    syscall_close(fd);
}

void cat_command(char **args) {
    char *path = args[1];
    int fd = syscall_open(path, O_RDONLY);
    if (fd < 0) {
        printf("Error: Failed to open file\n");
        return;
    }

    char buffer[256];
    int bytes_read = syscall_read(fd, buffer, sizeof(buffer));
    if (bytes_read < 0) {
        printf("Error: Failed to read file\n");
        return;
    }

    printf("%s\n", buffer);
    syscall_close(fd);
}

void touch_command(char **args) {
    char *path = args[1];
    int fd = syscall_open(path, O_WRONLY | O_CREAT);
    if (fd < 0) {
        printf("Error: Failed to create file\n");
        return;
    }

    syscall_close(fd);
}

void rm_command(char **args) {
    char *path = args[1];
    if (syscall_unlink(path) < 0) {
        printf("Error: Failed to remove file\n");
    }
}

void mkdir_command(char **args) {
    char *path = args[1];
    if (syscall_mkdir(path, 0) < 0) {
        printf("Error: Failed to create directory\n");
    }
}

void rmdir_command(char **args) {
    char *path = args[1];
    if (syscall_rmdir(path) < 0) {
        printf("Error: Failed to remove directory\n");
    }
}

void pwd_command() {
    syscall_getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);
}

void cd_command(char **args) {
    char *path = args[1];
    if (syscall_chdir(path) < 0) {
        printf("Error: Failed to change directory\n");
    } else {
        syscall_getcwd(cwd, sizeof(cwd));
    }
}

void clear_command() {
    term_clear_screen();
}

void write_command(char ** args) {
    char *path = args[1];
    int fd = syscall_open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        printf("Error: Failed to open file\n");
        return;
    }

    char buffer[256];
    printf("Enter text to write: ");
    fgets(buffer, 256, STDIN);
    syscall_write(fd, buffer, strlen(buffer));
    syscall_close(fd);
}

void test_command() {
    void *ptr = malloc(1024); // Allocate 1KB
    if (ptr == (void *)-1) {
        printf("Error: Failed to allocate memory\n");
    } else {
        printf("Allocated 1KB at %x\n", ptr);
        *(int *)ptr = 42; // Write to allocated memory
        printf("Wrote %d to allocated memory\n", *(int *)ptr);
        free(ptr); // Free the allocated memory
        printf("Freed allocated memory\n");
    }
}

void help_command() {
    printf("Commands:\n");
    printf("    echo <text> - Print text\n");
    printf("    ls [path] - List directory contents\n");
    printf("    cat <file> - Print file contents\n");
    printf("    touch <file> - Create a file\n");
    printf("    rm <file> - Remove a file\n");
    printf("    mkdir <dir> - Create a directory\n");
    printf("    rmdir <dir> - Remove a directory\n");
    printf("    pwd - Print working directory\n");
    printf("    cd <dir> - Change directory\n");
    printf("    clear - Clear the screen\n");
    printf("    test - For debugging\n");
    printf("    write <file> - Write to a file\n");
    printf("    history - Display command history\n");
    printf("    help - Display this help message\n");
    printf("    exit - Exit the shell\n");
}

command_t commands[] = {
    {"echo", echo_command},
    {"exit", exit_command},
    {"ls", ls_command},
    {"cat", cat_command},
    {"touch", touch_command},
    {"rm", rm_command},
    {"mkdir", mkdir_command},
    {"rmdir", rmdir_command},
    {"pwd", pwd_command},
    {"cd", cd_command},
    {"clear", clear_command},
    {"write", write_command},
    {"history", history_command},
    {"help", help_command},
    {"test", test_command},
    {NULL, NULL}
};

char *history_callback(int direction) {
    if (direction == -1) { // Up
        if (history_count == 0 || history_pos + 1 >= history_count) return NULL;
        history_pos++;
        return history_buffer[history_count - 1 - history_pos];
    } else if (direction == +1) { // Down
        if (history_pos <= 0) {
            history_pos = -1;
            return "";
        }
        history_pos--;
        return history_buffer[history_count - 1 - history_pos];
    }
    return NULL;
}

void add_to_history(char *buffer) {
    if (history_count < MAX_HISTORY) {
        strcpy(history_buffer[history_count++], buffer);
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(history_buffer[i], history_buffer[i + 1]);
        }
        strcpy(history_buffer[MAX_HISTORY - 1], buffer);
    }

    history_pos = history_count;
}

void parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " \n"); // Tokenize input by spaces
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        args[i] = strtok(NULL, " \n");
    }
    args[i] = NULL; // Null-terminate the argument list
}

int is_valid_command(char *command) {
    char *bin_commands[] = {"HELLO", "EDITOR"};
    for (size_t i = 0; i < sizeof(bin_commands) / sizeof(bin_commands[0]); i++) {
        if (strcmp(command, bin_commands[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

void execute_command(char **args) {
    if (!args[0]) return; // Empty input

    int input_fd = -1;
    int output_fd = -1;

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) { // Output redirection
            output_fd = syscall_open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC);
            if (output_fd < 0) {
                printf("Error: Failed to open file %s\n", args[i + 1]);
                return;
            }
            args[i] = NULL; // Remove redirection part from args
            break;
        } else if (strcmp(args[i], ">>") == 0) { // Append redirection
            output_fd = syscall_open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND);
            if (output_fd < 0) {
                printf("Error: Failed to open file %s\n", args[i + 1]);
                return;
            }
            args[i] = NULL;
            break;
        } else if (strcmp(args[i], "<") == 0) { // Input redirection
            input_fd = syscall_open(args[i + 1], O_RDONLY);
            if (input_fd < 0) {
                printf("Error: Failed to open file %s\n", args[i + 1]);
                return;
            }
            args[i] = NULL;
            break;
        }
    }

    // Redirect input if necessary
    if (input_fd != -1) {
        syscall_dup2(input_fd, STDIN);
        syscall_close(input_fd);
    }
    // Redirect output if necessary
    if (output_fd != -1) {
        syscall_dup2(output_fd, STDOUT);
        syscall_close(output_fd);
    }

    // Check for built-in commands
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(args[0], commands[i].name) == 0) {
            commands[i].func(args);
            return;
        }
    }

    if (!is_valid_command(args[0])) {
        printf("Unknown command: %s\n", args[0]);
        return;
    }
    
    char path[256];
    snprintf(path, sizeof(path), "/BIN/%s", args[0]);
    int pid = syscall_fork();
    if (pid == 0) {
        if (syscall_exec(path, args) < 0) {
            printf("Error: Failed to execute command\n");
            syscall_exit(1); // Exit child process
        }
    } else {
        int status;
        syscall_waitpid(pid, &status, 0);
    }
}

// void read_input(char *buffer) {
//     int cur = 0, len = 0;
//     char c;
//     while (1) {
//         if (syscall_read(STDIN, &c, 1) < 0) return;

//         switch (c) {
//             case '\n':
//                 buffer[len] = '\0';
//                 printf("\n");
//                 return;
//             case '\b':
//                 if (cur > 0) {
//                     cur--;
//                     len--;
//                     for (int i = cur; i < len; i++) {
//                         buffer[i] = buffer[i + 1];
//                     }
//                     buffer[len] = '\0';
//                     printf("\033[D\033[P");
//                 }
//                 break;
//             case '\033':
//                 char seq[2];
//                 if (syscall_read(STDIN, seq, 2) < 0) return;
//                 if (seq[0] == '[') {
//                     if (seq[1] == 'D' && cur > 0) {
//                         cur--;
//                         printf("\033[D");
//                     } else if (seq[1] == 'C' && cur < len) {
//                         cur++;
//                         printf("\033[C");
//                     } else if (seq[1] == 'A') {
//                         // load_history(buffer, &cur, &len, 1);
//                     } else if (seq[1] == 'B') {
//                         // load_history(buffer, &cur, &len, -1);
//                     }
//                 }
//                 break;
//             default:
//                 for (int i = len; i >= cur; i--) {
//                     buffer[i] = buffer[i - 1];
//                 }
//                 buffer[cur] = c;
//                 len++;
//                 cur++;

//                 printf("\033[@"); // Insert space at cursor
//                 printf("%c", c);  // Write the new character
//                 break;
//         }

//         if (cur > len) cur = len;
//         if (cur < 0) cur = 0;
//         if (len >= MAX_INPUT - 1) break;
//     }

//     buffer[len] = '\0';
// }

void main() {
    char buffer[MAX_INPUT];
    char *args[MAX_ARGS];

    syscall_getcwd(cwd, sizeof(cwd));
	printf("\n");
    while (1) {
        printf("%s$ ", cwd);
        // fgets(buffer, sizeof(buffer), stdin);
        // read_input(buffer);
        readline(buffer, sizeof(buffer));

        // printf("Command: %s\n", buffer);
        parse_input(buffer, args);
        execute_command(args);
        add_to_history(buffer);
    }

    syscall_exit(0);
}