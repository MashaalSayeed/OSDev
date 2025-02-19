#include "user/stdio.h"
#include "user/umain.h"

void user_program() {
    int i = 0;
    while (1) {
        // Print a message using a syscall
        char buffer[80];
        // printf("Enter a string: ");
        // i++;

        puts("Enter a string: ");
        fgets(0, buffer, 80);
    }
}