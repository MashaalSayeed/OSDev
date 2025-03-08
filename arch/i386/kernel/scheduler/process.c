#include "kernel/process.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include <stdbool.h>

#define USER_STACK_BASE  0xB0000000

process_t *process_list = NULL;
process_t *current_process = NULL;
process_t *init_process = NULL;
size_t pid_counter = 0;
bool scheduler_started = false;

extern page_directory_t* kpage_dir;
extern void switch_context(registers_t* context);

// Bump Allocator
size_t allocate_pid() {
    return pid_counter++;
}

void idle_process() {
    while (1) {
    }
}

void scheduler_init() {
    init_process = create_process("init", idle_process);
    // add_process(init_process);
}

void schedule(registers_t* context) {
    if (!process_list) return;

    // // Save the context of the current processs
    if (current_process != NULL && context != NULL && scheduler_started == true) {
        current_process->context = *context;
        current_process->status = READY;
    }

    scheduler_started = true;
    process_t *next_process = current_process ? current_process : process_list;
    do {
        next_process = next_process->next ? next_process->next : process_list;
    } while (next_process->status != READY && next_process != current_process);

    if (next_process == current_process && (current_process == NULL || current_process->status != READY)) {
        next_process = init_process;  // Always have a fallback
    }
    
    // printf("Current process %d (%d) | Next Process: %d (%d)\n", current_process->pid, current_process->status, next_process->pid, next_process->status);
    if (next_process != current_process || next_process->status == READY) {
        current_process = next_process;
        current_process->status = RUNNING;
        
        // Restore context and switch page directory
        switch_page_directory(current_process->root_page_table);
        switch_context(&current_process->context);
    } else {
        current_process->status = RUNNING;
    }
}


process_t* create_process(char *process_name, void (*entry_point)()) {
    process_t *new_process = (process_t *)kmalloc(sizeof(process_t));
    if (!new_process) {
        printf("Error: Failed to allocate memory for process %s\n", process_name);
        return NULL;
    }

    new_process->pid = allocate_pid();
    strncpy(new_process->process_name, process_name, PROCESS_NAME_MAX_LEN);
    new_process->root_page_table = clone_page_directory(kpage_dir);
    
    uint32_t stack = USER_STACK_BASE - (new_process->pid * PROCESS_STACK_SIZE);
    alloc_page(get_page(stack - PROCESS_STACK_SIZE, 1, new_process->root_page_table), 0x7);

    new_process->stack = (void *)stack - PROCESS_STACK_SIZE;
    new_process->heap_start = NULL;
    new_process->brk = NULL;
    new_process->heap_end = NULL;

    // Initialize process context
    new_process->context.eip = (uint32_t)entry_point;
    new_process->context.eflags = 0x202;
    new_process->context.esp = (uint32_t)stack;
    new_process->context.ebp = new_process->context.esp;

    new_process->context.cs = USER_CS;
    new_process->context.ds = USER_DS;
    new_process->context.ss = USER_SS;
    new_process->context.eax = 0;
    new_process->context.ebx = 0;
    new_process->context.ecx = 0;
    new_process->context.edx = 0;
    new_process->context.edi = 0;
    new_process->context.esi = 0;

    new_process->status = READY;
    return new_process;
}

void add_process(process_t *process) {
    if (!process_list) {
        process_list = process;
        current_process = process;
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

    if (process == process_list) {
        process_list = process_list->next;
    }

    process_t *temp = process_list;
    while (temp->next != process) {
        temp = temp->next;
    }

    temp->next = process->next;
    if (process == current_process) {
        current_process = NULL;
    }

    // Free the process stack and process structure
    free_page(get_page((uint32_t)process->stack, 0, process->root_page_table));
    free_page_directory(process->root_page_table);
    kfree(process);
    schedule(NULL);
}

void kill_current_process() {
    kill_process(current_process);
}

process_t* get_current_process() {
    return current_process;
}

void print_process_list() {
    process_t *temp = process_list;
    if (temp == NULL) return;
    do {
        printf("PID: %d, Name: %s, Status: %d\n", temp->pid, temp->process_name, temp->status);
        temp = temp->next;
    } while (temp != process_list);
}