#include "user/stdio.h"
#include "user/umain.h"
#include "libc/string.h"
#include "user/syscall.h"

#define MAX_INPUT 256
#define MAX_ARGS 10

void parse_input(char *input, char **args) {
    int i = 0;
    args[i] = strtok(input, " \n"); // Tokenize input by spaces
    while (args[i] != NULL && i < MAX_ARGS - 1) {
        i++;
        args[i] = strtok(NULL, " \n");
    }
    args[i] = NULL; // Null-terminate the argument list
}

void execute_command(char **args) {
    if (args[0] == NULL) return; // Empty input, do nothing

    printf("Executing command: %s\n", args[0]);
}

void user_program() {
    int i = 0;
    char buffer[80];

    while (1) {
        // Print a message using a syscall
        puts("Enter a string: ");
        fgets(0, buffer, 80);
        printf("You entered: %s\n", buffer);

        if (strcmp(buffer, "exit") == 0) {
            break;
        }
    }

    exit(0);
}