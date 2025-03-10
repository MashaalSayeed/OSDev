#include "kernel/process.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include "kernel/elf.h"
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
    int a = 1 / 0;
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

void* sbrk(process_t *proc, int increment) {
    if (increment == 0) return proc->brk;

    void *old_brk = proc->brk;
    void *new_brk = proc->brk + increment;

    if (increment > 0) {
        // Expand heap by allocating pages
        while (proc->brk < new_brk) {
            alloc_page(get_page(proc->brk, 1, proc->root_page_table), 0x7);
            proc->brk += PAGE_SIZE;
        }
    } 
    else if (increment < 0) {
        // Shrink heap (optional)
        while (proc->brk > new_brk) {
            free_page(get_page(proc->brk - PAGE_SIZE, 0, proc->root_page_table));
            proc->brk -= PAGE_SIZE;
        }
    }

    // proc->brk = new_brk;
    return old_brk;
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
    
    // TODO: stack = USER_STACK_BASE
    // uint32_t stack = USER_STACK_BASE - (new_process->pid * PROCESS_STACK_SIZE);
    new_process->stack = (void *)USER_STACK_BASE - PROCESS_STACK_SIZE;
    map_memory(new_process->root_page_table, new_process->stack, 0, PROCESS_STACK_SIZE, 0x7);

    new_process->heap_start = USER_HEAP_START;
    new_process->brk = new_process->heap_start;
    new_process->heap_end = new_process->heap_start;

    strncpy(new_process->cwd, "/", 1);

    // Initialize process context
    new_process->context.eip = (uint32_t)entry_point;
    new_process->context.eflags = 0x202;
    new_process->context.esp = (uint32_t)USER_STACK_BASE;
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
    // TODO: unmap all pages and heap
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

int fork() {
    process_t *parent = get_current_process();
    process_t *child = (process_t *)kmalloc(sizeof(process_t));
    if (!child) {
        printf("Error: Failed to allocate memory for child process\n");
        return -1;
    }

    // Copy parent process info
    child->pid = allocate_pid();
    printf("Forking process %d (%s) to %d\n", parent->pid, parent->process_name, child->pid);

    strncpy(child->process_name, parent->process_name, PROCESS_NAME_MAX_LEN);
    child->status = READY;
    child->heap_start = parent->heap_start;
    child->heap_end = parent->heap_end;
    child->brk = parent->brk;
    strncpy(child->cwd, parent->cwd, sizeof(parent->cwd));

    // Copy page directory
    child->root_page_table = clone_page_directory(parent->root_page_table);
    child->stack = (void *)USER_STACK_BASE - PROCESS_STACK_SIZE;

    // Copy process context
    memcpy(&child->context, &parent->context, sizeof(registers_t));
    child->context.eax = 0; // Return value for child process

    uint32_t esp_diff = USER_STACK_BASE - parent->context.esp;
    child->context.esp = (uint32_t)child->stack + (PROCESS_STACK_SIZE - esp_diff);
    child->context.ebp = (uint32_t)child->stack + (PROCESS_STACK_SIZE - USER_STACK_BASE + parent->context.ebp);

    add_process(child);
    return child->pid;
}

int exec(char *path) {
    elf_header_t* elf = load_elf(path);
    if (!elf) {
        printf("Failed to load ELF file\n");
        return -1;
    }

    process_t *proc = get_current_process();
    page_directory_t* new_page_dir = clone_page_directory(kpage_dir);
    free_page_directory(proc->root_page_table);

    proc->root_page_table = new_page_dir;
    switch_page_directory(proc->root_page_table);

    proc->stack = (void *)USER_STACK_BASE - PROCESS_STACK_SIZE;
    map_memory(proc->root_page_table, proc->stack, 0, PROCESS_STACK_SIZE, 0x7);

    memset(&proc->context, 0, sizeof(registers_t));
    proc->context.eip = elf->entry;
    proc->context.esp = USER_STACK_BASE;

    asm volatile (
        "mov %0, %%esp\n"
        "jmp *%1\n"
        :
        : "r"(proc->context.esp), "r"(proc->context.eip)
    );

    return 0;
}
