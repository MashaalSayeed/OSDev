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

        // Restore context and switch page directory
        // printf("Switching to thread %d (%s)\n", current_thread->tid, current_thread->thread_name);
        switch_page_directory(current_thread->owner->root_page_table);
        switch_context(&current_thread->context);
    } else {
        current_thread->owner->status = RUNNING;
    }
}

process_t* get_current_process() {
    return current_thread->owner;
}

process_t* get_process(size_t pid) {
    thread_t *temp = thread_list;
    if (temp == NULL) return NULL;
    do {
        if (temp->owner->pid == pid) return temp->owner;
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

void remove_thread(thread_t *thread) {
    if (!thread_list || !thread) return;

    if (thread_list == thread) {
        if (thread->next_global == thread_list) {
            // Only one thread in the list
            thread_list = NULL;
            current_thread = NULL;
            return;
        }

        thread_list = thread->next_global;
    }

    thread_t *prev = thread_list;
    while (prev->next_global != thread) {
        prev = prev->next_global;
    }

    prev->next_global = thread->next_global;
}

void add_process(process_t *process) {
    if (!process) return;

    thread_t *thread = process->thread_list;
    while (thread != NULL) {
        add_thread(thread);
        thread = thread->next;
    }
}

thread_t *pick_next_thread() {
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
    printf("Thread List:\n");
    if (temp == NULL) {
        printf("No threads available.\n");
        return;
    }

    printf("--------------------------------------------------\n");
    printf("| TID | Thread Name          | Status |\n");
    printf("--------------------------------------------------\n");
    do {
        printf("|%4d | %40s | s: %2d |\n", temp->tid, temp->thread_name, temp->status);
        temp = temp->next_global;
    } while (temp != thread_list);
    printf("--------------------------------------------------\n");
    printf("Current Thread: TID: %d, Name: %s, Status: %d\n", 
           current_thread->tid, current_thread->thread_name, current_thread->status);
    printf("Current Process: PID: %d, Name: %s, Status: %d\n", 
           current_thread->owner->pid, current_thread->owner->process_name, current_thread->owner->status);
    printf("--------------------------------------------------\n");
}