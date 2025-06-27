#include "kernel/process.h"
#include "kernel/isr.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include <stdbool.h>

extern void switch_context(registers_t* context);

process_t *init_process = NULL;
process_t *current_process = NULL;
process_t *process_list = NULL;

bool scheduler_started = false;

void idle_process() {
    while (1) {
        asm volatile ("hlt");
    }
}

void scheduler_init() {
    init_process = create_process("init", idle_process, PROCESS_FLAG_KERNEL);
    init_process->status = INIT;
    add_process(init_process);
}

void schedule(registers_t* context) {
    if (!process_list) return;

    // // Save the context of the current processs
    scheduler_started = true;
    // printf("Scheduling process\n");
    if (current_process != NULL && context != NULL) {
        current_process->context = *context;
        if (current_process->status == RUNNING) current_process->status = READY;
    }

    process_t *next_process = current_process ? current_process : process_list;
    do {
        next_process = next_process->next ? next_process->next : process_list;
    } while (next_process->status != READY && next_process != current_process);

    if (next_process == current_process && (current_process == NULL || current_process->status != READY)) {
        next_process = init_process;  // Always have a fallback
    }
    
    // printf("Current process (%d) | Next Process: %d\n", current_process->pid, next_process->pid);
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

process_t* get_current_process() {
    return current_process;
}

process_t* get_process(size_t pid) {
    process_t *temp = process_list;
    if (temp == NULL) return NULL;
    do {
        if (temp->pid == pid) return temp;
        temp = temp->next;
    } while (temp != process_list);

    return NULL;
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

void print_process_list() {
    process_t *temp = process_list;
    if (temp == NULL) return;
    do {
        printf("PID: %d, Name: %s, Status: %d\n", temp->pid, temp->process_name, temp->status);
        temp = temp->next;
    } while (temp != process_list);
}