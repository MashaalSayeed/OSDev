#include "kernel/process.h"
#include "kernel/isr.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include "drivers/pit.h"

#include <stdbool.h>

extern void switch_context(uint32_t* prev, thread_context_t* context);
extern void switch_task(uint32_t* prev, uint32_t* next);

// process_t *idle_process = NULL;
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
//0xc0403ffc; 0xc0403fdc; 0xc0403fe4
void schedule(registers_t* context) {
    if (!thread_list || !current_thread) return;

    thread_t *prev_thread = current_thread;
    if (prev_thread->status == RUNNING) prev_thread->status = READY;

    thread_t *next_thread = pick_next_thread(current_thread);
    if (next_thread && next_thread != current_thread) {
        // printf("Switching from thread: %s (TID: %d) to thread: %s (TID: %d) %x -> %x\n",
        //        prev_thread->thread_name, prev_thread->tid,
        //        next_thread->thread_name, next_thread->tid,
        //        &prev_thread->kernel_stack, &next_thread->kernel_stack);
        if (context != NULL) {
            // printf("[TID: %d] Saving context: eip %x; ebp %x; useless %x\n", 
            //     prev_thread->tid, context->eip, context->ebp, context->err_code);
            prev_thread->context.eip = context->eip;
            prev_thread->context.user_esp = context->esp;
            prev_thread->context.ebp = context->ebp;
            prev_thread->context.eflags = context->eflags;

            prev_thread->context.edi = context->edi;
            prev_thread->context.esi = context->esi;
            prev_thread->context.eax = context->eax;
            prev_thread->context.ebx = context->ebx;
            prev_thread->context.ecx = context->ecx;
            prev_thread->context.edx = context->edx;
        }

        current_thread = next_thread;
        current_thread->status = RUNNING;

        // Restore context and switch page directory
        if (prev_thread->owner->root_page_table != next_thread->owner->root_page_table) {
            switch_page_directory(next_thread->owner->root_page_table);
        }

        if (current_thread->owner->is_kernel_process) {
            switch_task(&prev_thread->kernel_stack, current_thread->kernel_stack);
        } else {
            switch_task(&prev_thread->kernel_stack, current_thread->kernel_stack);
            asm volatile ("mov %%esp, %0" : "=r"(prev_thread->context.esp));
            switch_context(&prev_thread->kernel_stack, &current_thread->context);
        }
    } else {
        current_thread->status = RUNNING;
    }
}

__attribute__((naked))
static void jmp_to_kernel_thread_context(thread_context_t *context) {
    __asm__ volatile (
        "mov 4(%esp), %eax\n"

        "mov 12(%eax), %esp\n" // ESP
        "mov 8(%eax), %ebp\n" // EBP
        "mov 32(%eax), %ecx\n" // EIP
        "sti\n"

        "jmp *%ecx"
    );
}

void jmp_to_kernel_thread(thread_t *thread) {
    printf("Switching to kernel thread: %s (TID: %d) %x\n", thread->thread_name, thread->tid, thread->owner);
    jmp_to_kernel_thread_context(&thread->context);
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

    // if (thread == current_thread) current_thread = thread->next_global;
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