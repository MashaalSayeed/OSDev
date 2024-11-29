#include "kernel/process.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "libc/stdio.h"
#include "libc/string.h"

#define STACK_SIZE 4096

process_t *process_list = NULL;
process_t *current_process = NULL;
size_t pid_counter = 0;

extern void switch_context(registers_t* context);

// Bump Allocator
size_t allocate_pid() {
    return pid_counter++;
}

void idle_process() {
    while (1) {
        asm volatile("hlt");
    }
}

void scheduler_init() {
    process_t* init_process = create_process("init", &idle_process);
    add_process(init_process);
}

void schedule(registers_t* context) {
    if (!process_list) return;

    // Save the context of the current processs
    current_process->context = *context;
    current_process->status = READY;


    // Find the next READY process
    process_t *next_process = current_process->next;
    while (next_process->status != READY) {
        next_process = next_process->next;
    }

    if (next_process != current_process) {
        // printf("Switching from '%s' to '%s'\n", current_process->process_name, next_process->process_name);
        // Update the current process
        current_process = next_process;
        current_process->status = RUNNING;

        // Restore context and switch page directory
        switch_context(&current_process->context);
    } else {
        current_process->status = RUNNING;
    }
}

process_t* create_process(char *process_name, void (*entry_point)()) {
    // Allocate memory for the new process structure
    process_t *new_process = (process_t *)kmalloc(sizeof(process_t));

    if (!new_process) {
        printf("Error: Failed to allocate memory for process %s\n", process_name);
        return NULL;
    }

    // Assign process properties
    new_process->pid = allocate_pid();
    new_process->status = READY;
    strncpy(new_process->process_name, process_name, PROCESS_NAME_MAX_LEN);

    // Initialize process context
    new_process->context.eip = (uint32_t)entry_point;
    new_process->context.eflags = 0x202;                       // Enable interrupts
    new_process->context.esp = (uint32_t)kmalloc(4096) + 4096; // Allocate and set stack pointer
    new_process->context.cs = KERNEL_CS;
    new_process->context.ds = KERNEL_DS;
    new_process->context.ss = KERNEL_SS;

    // Initialize other general-purpose registers to 0 (optional but safer)
    new_process->context.eax = 0;
    new_process->context.ebx = 0;
    new_process->context.ecx = 0;
    new_process->context.edx = 0;
    new_process->context.edi = 0;
    new_process->context.esi = 0;
    new_process->context.ebp = 0;

    // printf("Process '%s' created with PID %d\n", process_name, new_process->pid);
    return new_process;
}

void add_process(process_t *process) {
    if (!process_list) {
        process_list = process;
        current_process = process_list;
    } else {
        process_t *temp = process_list;
        while (temp->next != process_list) {
            temp = temp->next;
        }
        temp->next = process;
    }

    // Circular linked list
    process->next = process_list;
}

void kill_process(process_t *process) {
    process->status = TERMINATED;
    kfree((uint32_t *)(process->context.esp - 4096));
    kfree(process);
}

void print_process_list() {
    process_t *temp = process_list;
    do {
        printf("PID: %d, Name: %s, Status: %d\n", temp->pid, temp->process_name, temp->status);
        temp = temp->next;
    } while (temp != process_list);
}