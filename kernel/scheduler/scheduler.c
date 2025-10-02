#include "kernel/process.h"
#include "kernel/isr.h"
#include "kernel/gdt.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include "drivers/pit.h"

#include <stdbool.h>

extern void switch_task(uintptr_t* prev, uintptr_t next);
extern struct tss_entry tss_entry;

thread_t *thread_list = NULL;
thread_t *current_thread = NULL;

bool scheduler_started = false;

void idle_process() {
    while (1) {
        printf("HELLO FROM IDLE\n");
        asm volatile ("hlt");
    }
}

void scheduler_init() {
    init_timer(100);
    // init_process = create_process("init", idle_process, PROCESS_FLAG_KERNEL);
    // init_process->status = INIT;
    // add_process(init_process);
    // thread_context_t* ctx = &init_process->main_thread->context;
    // thread_t* main_thread = init_process->main_thread;
    // jmp_to_kernel_thread(&main_thread->context);
}

void schedule(registers_t* context) {
    if (!thread_list || !current_thread) return;

    // Deliver pending signals before switching
    signal_deliver(current_thread->owner);

    thread_t *prev_thread = current_thread;
    if (prev_thread->status == RUNNING) prev_thread->status = READY;

    thread_t *next_thread = pick_next_thread(current_thread);
    if (next_thread && next_thread != current_thread) {
        current_thread = next_thread;
        current_thread->status = RUNNING;

        // Restore context and switch page directory
        if (prev_thread->owner->root_page_table != next_thread->owner->root_page_table) {
            switch_page_directory(next_thread->owner->root_page_table);
        }

        tss_entry.esp0 = (uint32_t)current_thread->kernel_stack + PROCESS_STACK_SIZE;
        switch_task(&prev_thread->esp, current_thread->esp);
    } else {
        current_thread->status = RUNNING;
    }
}

__attribute__((naked))
static void jmp_to_kernel_thread_context(thread_t *thread) {
    __asm__ volatile (
        "mov 4(%esp), %eax\n"

        "mov 8(%eax), %esp\n" // ESP
        "mov 12(%eax), %ebp\n" // EBP
        "mov 4(%eax), %ecx\n" // EIP
        "sti\n"

        "jmp *%ecx"
    );
}

void jmp_to_kernel_thread(thread_t *thread) {
    printf("Switching to kernel thread: %s (TID: %d) %x\n", thread->thread_name, thread->tid, thread->owner);
    tss_entry.esp0 = (uint32_t)thread->kernel_stack + PROCESS_STACK_SIZE;
    jmp_to_kernel_thread_context(thread);
}

process_t* get_current_process() {
    return current_thread->owner;
}

thread_t* get_current_thread() {
    return current_thread;
}

thread_t* get_thread(size_t tid) {
    thread_t *temp = thread_list;
    if (temp == NULL) return NULL;
    do {
        if (temp->tid == tid) return temp;
        temp = temp->next_global;
    } while (temp != thread_list);

    return NULL;
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
    if (current_thread == NULL) current_thread = thread;
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
    while (next && next != current_thread) {
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
    printf("| TID | Thread Name          | Status | Proc |\n");
    printf("--------------------------------------------------\n");
    do {
        printf("|%4d | %40s | s: %2d | %4d |\n", temp->tid, temp->thread_name, temp->status, temp->owner->pid);
        temp = temp->next_global;
    } while (temp != thread_list);
    printf("--------------------------------------------------\n");
    printf("Current Thread: TID: %d, Name: %s, Status: %d\n", 
           current_thread->tid, current_thread->thread_name, current_thread->status);
    printf("Current Process: PID: %d, Name: %s, Status: %d\n", 
           current_thread->owner->pid, current_thread->owner->process_name, current_thread->owner->status);
    printf("--------------------------------------------------\n");
}