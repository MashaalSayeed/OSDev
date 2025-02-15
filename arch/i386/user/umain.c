#include "user/stdio.h"
#include "user/umain.h"

void user_program() {
    int i = 0;
    while (1) {
        // Print a message using a syscall
        printf("Hello from user mode!\n");
    }
}