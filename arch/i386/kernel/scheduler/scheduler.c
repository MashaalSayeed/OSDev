#include "kernel/process.h"
#include "kernel/isr.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include <stdbool.h>

extern void switch_context(registers_t* context);

process_t *init_process = NULL;
thread_t *thread_list = NULL;
thread_t *current_thread = NULL;

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
    if (!thread_list) return;

    // // Save the context of the current processs
    scheduler_started = true;
    // printf("Scheduling process\n");
    if (current_thread != NULL && context != NULL) {
        current_thread->context = *context;
        if (current_thread->status == RUNNING) current_thread->status = READY;
    }

    thread_t *next_thread = pick_next_thread(current_thread);

    // printf("Current thread (%d) | Next Thread: %d\n", current_thread->tid, next_thread->tid);
    if (next_thread != current_thread || next_thread->status == READY) {
        current_thread = next_thread;
        current_thread->status = RUNNING;

        // current_process->threads->status = RUNNING;
        registers_t *thread_context = &current_thread->context;

        // Restore context and switch page directory
        switch_page_directory(current_thread->parent->root_page_table);
        switch_context(thread_context);
    } else {
        current_thread->parent->status = RUNNING;
    }
}

process_t* get_current_process() {
    return current_thread->parent;
}

process_t* get_process(size_t pid) {
    thread_t *temp = thread_list;
    if (temp == NULL) return NULL;
    do {
        if (temp->parent->pid == pid) return temp->parent;
        temp = temp->next_global;
    } while (temp != thread_list);

    return NULL;
}

void add_thread(thread_t *thread) {
    if (!thread_list) {
        thread_list = thread;
        current_thread = thread;
    } else {
        thread_t *temp = thread_list;
        while (temp->next_global != thread_list) {
            temp = temp->next_global;
        }
        temp->next_global = thread;
    }

    // Circular linked list
    thread->next_global = thread_list;
}

void add_process(process_t *process) {
    if (!process) return;

    thread_t *thread = process->threads;
    add_thread(thread);
}

thread_t *pick_next_thread(thread_t *current_thread) {
    if (!current_thread) return NULL;

    thread_t *next = current_thread->next_global;
    while (next != current_thread) {
        if (next->status == READY) return next;
        next = next->next_global;
    }

    return current_thread->status == READY ? current_thread : NULL;
}

void print_thread_list() {
    thread_t *temp = thread_list;
    if (temp == NULL) return;
    do {
        printf("TID: %d, Name: %s, Status: %d\n", temp->tid, temp->thread_name, temp->status);
        temp = temp->next_global;
    } while (temp != thread_list);
}