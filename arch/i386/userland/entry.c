void user_program() {
    while (1) {
        // Print a message using a syscall
        syscall_print("Hello from user mode!\n");
    }
}