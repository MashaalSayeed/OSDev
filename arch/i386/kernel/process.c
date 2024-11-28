#include "kernel/process.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "libc/stdio.h"
#include "libc/string.h"

#define STACK_SIZE 4096

process_t *process_list = NULL;
process_t *current_process = NULL;
size_t pid_counter = 0;

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
    process_list = create_process("init", &idle_process);
    process_list->status = RUNNING;

    current_process = process_list;
}

registers_t* schedule(registers_t* context) {
    // Save the context of the current process
    memcpy(&current_process->context, context, sizeof(registers_t));

    // Find the next READY process
    process_t *next_process = current_process->next ? current_process->next : process_list;
    while (next_process->status != READY) {
        next_process = next_process->next ? next_process->next : process_list;
    }

    // Update the current process
    current_process = next_process;
    current_process->status = RUNNING;

    // Restore context and switch page directory
    memcpy(context, &current_process->context, sizeof(registers_t));
    // switch_page_directory(current_process->root_page_table);

    return current_process->context;
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

    // Allocate kernel stack for the process
    new_process->context = (registers_t *)kmalloc(sizeof(registers_t));
    if (!new_process->context) {
        printf("Error: Failed to allocate memory for process %s stack\n", process_name);
        kfree(new_process);
        return NULL;
    }

    // Set up process context
    memset(new_process->context, 0, sizeof(registers_t)); // Clear the context memory
    new_process->context->eip = (uint32_t)entry_point;    // Entry point of the process
    new_process->context->cs = 0x08;                     // Kernel code segment
    new_process->context->eflags = 0x202;                // Enable interrupts
    new_process->context->esp = (uint32_t)kmalloc(4096) + 4096; // Allocate and set stack pointer
    new_process->context->ss = 0x10;                     // Kernel data segment

    // Set up paging for the process (optional, depending on your implementation)
    // new_process->root_page_table = clone_page_directory();
    // if (!new_process->root_page_table) {
    //     printf("Error: Failed to allocate page directory for process %s\n", process_name);
    //     kfree(new_process->context);
    //     kfree(new_process);
    //     return NULL;
    // }

    // Add the process to the process list
    if (!process_list) {
        process_list = new_process;
    } else {
        process_t *current = process_list;
        while (current->next) {
            current = current->next;
        }
        current->next = new_process;
    }
    new_process->next = NULL;

    printf("Process %s created with PID %d\n", process_name, new_process->pid);
    return new_process;
}

void kill_process(process_t *process) {
    process->status = TERMINATED;
    kfree(process->context);
    kfree(process);
}

