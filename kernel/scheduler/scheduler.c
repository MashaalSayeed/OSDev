#include "kernel/process.h"
#include "kernel/isr.h"
#include "kernel/gdt.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include "drivers/pit.h"

#include <stdbool.h>

extern void switch_task(uintptr_t* prev, uintptr_t next);
extern struct tss_entry tss_entry;
extern page_directory_t* kpage_dir;

thread_t *thread_list = NULL;
thread_t *current_thread = NULL;
thread_t *idle_thread_ptr = NULL;

static page_directory_t *thread_page_directory(thread_t *thread) {
    if (!thread || !thread->owner) {
        return kpage_dir;
    }

    return thread->owner->root_page_table;
}

void idle_thread(void) {
    asm volatile ("sti");
    while (1) {
        asm volatile ("hlt");
    }
}

void scheduler_init() {
    pit_init(100);
    idle_thread_ptr = create_thread(NULL, idle_thread, "idle");
    add_thread(idle_thread_ptr);
}

void schedule(registers_t* context) {
    if (!thread_list || !current_thread) {
        kprintf(DEBUG, "schedule: no threads, halting\n");
        return;
    }

    thread_t *prev_thread = current_thread;
    if (prev_thread->status == RUNNING) prev_thread->status = READY;

    thread_t *next_thread = pick_next_thread();
    if (next_thread && next_thread != current_thread) {
        current_thread = next_thread;
        current_thread->status = RUNNING;

        // Restore context and switch page directory
        page_directory_t *prev_pd = thread_page_directory(prev_thread);
        page_directory_t *next_pd = thread_page_directory(next_thread);
        if (prev_pd != next_pd) {
            switch_page_directory(next_pd);
        }

        tss_entry.esp0 = (uint32_t)current_thread->kernel_stack + PROCESS_STACK_SIZE;
        switch_task(&prev_thread->esp, current_thread->esp);
    } else if (next_thread == current_thread) {
        current_thread->status = RUNNING;
    } else {
        // pick_next_thread() returned NULL, meaning every thread is WAITING or TERMINATED.
        // In this case, we can halt the cpu and wait for the next timer interrupt to wake us up. 
        // This can happen if all threads are waiting for some event (e.g. I/O) to complete.
        kprintf(ERROR, "schedule: no ready threads, halting\n");
        asm volatile ("sti; nop; hlt");
    }
}

void jmp_to_kernel_thread(thread_t *thread) {
    printf("Switching to kernel thread: %s (TID: %d) %x\n", thread->thread_name, thread->tid, thread->owner);
    tss_entry.esp0 = (uint32_t)thread->kernel_stack + PROCESS_STACK_SIZE;
    uint32_t dummy;
    asm volatile("sti");
    switch_task((uintptr_t *)&dummy, thread->esp);
}

process_t* get_current_process() {
    if (!current_thread) return NULL;
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

void add_thread(thread_t *thread) {
    if (!thread) return;

    if (thread_list) {
        thread_t *temp = thread_list;
        do {
            if (temp == thread) {
                return; // Already in the list, avoid duplicate insertion
            }
            temp = temp->next_global;
        } while (temp != thread_list);
    }

    if (!thread_list) {
        thread_list = thread;
        thread->next_global = thread;
    } else {
        thread_t *temp = thread_list;
        while (temp->next_global != thread_list) {
            temp = temp->next_global;
        }
        temp->next_global = thread;
        thread->next_global = thread_list;
    }

    if (current_thread == NULL) {
        current_thread = thread;
    }
}

void remove_thread(thread_t *thread) {
    if (!thread_list || !thread) return;

    // Check if the thread is actually in the list first to avoid infinite loops/corruption
    bool found = false;
    thread_t *temp = thread_list;
    do {
        if (temp == thread) {
            found = true;
            break;
        }
        temp = temp->next_global;
    } while (temp != thread_list);

    if (!found) return; // Thread not in the list, nothing to do

    if (thread_list == thread) {
        if (thread->next_global == thread_list) {
            // Only one thread in the list
            thread_list = NULL;
            if (current_thread == thread) {
                current_thread = NULL;
            }
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

void schedule_process_threads(process_t *process) {
    if (!process) return;

    thread_t *thread = process->thread_list;
    while (thread != NULL) {
        add_thread(thread);
        thread = thread->next;
    }
}

thread_t *pick_next_thread() {
    if (!thread_list) return NULL;

    uint32_t ticks = pit_get_ticks();

    // Start scanning from after current_thread, or from head if terminated
    thread_t *start;
    if (!current_thread || current_thread->status == TERMINATED) {
        start = thread_list;
    } else {
        start = current_thread->next_global;
    }

    // Scan all threads starting from start, wrapping once
    thread_t *t = start;
    do {
        if (t->status == SLEEPING && t->wakeup_tick <= ticks) {
            t->status = READY;
        }
        // Skip the idle thread during normal search so it is only picked as fallback
        if (t->status == READY && t != idle_thread_ptr) {
            return t;
        }

        t = t->next_global;
    } while (t != start);

    // If no other thread is ready, run the idle thread
    if (idle_thread_ptr && idle_thread_ptr->status == READY) {
        return idle_thread_ptr;
    }

    return NULL;
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
        int proc_pid = temp->owner ? temp->owner->pid : -1;
        printf("|%4d | %40s | s: %2d | %4d |\n", temp->tid, temp->thread_name, temp->status, proc_pid);
        temp = temp->next_global;
    } while (temp != thread_list);

    if (current_thread) {
        printf("--------------------------------------------------\n");
        printf("Current Thread: TID: %d, Name: %s, Status: %d\n", 
            current_thread->tid, current_thread->thread_name, current_thread->status);

        if (current_thread->owner) {
            printf("Current Process: PID: %d, Name: %s, Status: %d\n", 
                current_thread->owner->pid, current_thread->owner->process_name, current_thread->owner->status);
        }
        printf("--------------------------------------------------\n");
    } else {
        printf("No current thread.\n");
    }
}