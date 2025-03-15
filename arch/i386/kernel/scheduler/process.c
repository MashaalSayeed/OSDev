#include "kernel/process.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include "kernel/elf.h"
#include "kernel/vfs.h"
#include <stdbool.h>

#define USER_STACK_BASE  0xB0000000

size_t pid_counter = 1;

extern page_directory_t* kpage_dir;
extern process_t* process_list;
extern process_t* current_process;
extern void sys_exit(int status);

extern vfs_file_t* console_fds[3];

// Bump Allocator
size_t allocate_pid() {
    return pid_counter++;
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

process_t* create_process(char *process_name, void (*entry_point)(), uint32_t flags) {
    process_t *new_process = (process_t *)kmalloc(sizeof(process_t));
    if (!new_process) {
        printf("Error: Failed to allocate memory for process %s\n", process_name);
        return NULL;
    }

    new_process->pid = allocate_pid();
    strncpy(new_process->process_name, process_name, PROCESS_NAME_MAX_LEN);
    new_process->root_page_table = clone_page_directory(kpage_dir);
    
    new_process->stack = (void *)USER_STACK_BASE - PROCESS_STACK_SIZE;
    map_memory(new_process->root_page_table, new_process->stack, 0, PROCESS_STACK_SIZE, 0x7);

    new_process->heap_start = USER_HEAP_START;
    new_process->brk = new_process->heap_start;
    new_process->heap_end = new_process->heap_start;

    strncpy(new_process->cwd, "/home", sizeof(new_process->cwd));

    new_process->fds[0] = vfs_get_file(0);
    new_process->fds[1] = vfs_get_file(1);
    new_process->fds[2] = vfs_get_file(2);

    // Initialize process context
    memset(&new_process->context, 0, sizeof(registers_t));
    new_process->context.eip = (uint32_t)entry_point;
    new_process->context.eflags = 0x202;
    new_process->context.esp = (uint32_t)USER_STACK_BASE;
    new_process->context.ebp = new_process->context.esp;

    if (flags & PROCESS_FLAG_USER) {
        new_process->context.cs = USER_CS;
        new_process->context.ds = USER_DS;
        new_process->context.ss = USER_SS;
    } else {
        new_process->context.cs = KERNEL_CS;
        new_process->context.ds = KERNEL_DS;
        new_process->context.ss = KERNEL_DS;
    }

    new_process->status = READY;
    return new_process;
}

void kill_process(process_t *process, int status) {
    if (!process || process->status == TERMINATED) return;
    process->status = TERMINATED;

    // wake up parent if waiting
    if (process->parent && process->parent->status == WAITING) {
        process->parent->status = READY;
        process->parent->context.eax = status; // TODO: save child status
        // TODO: save child status
        cleanup_process(process);
    } else {
        cleanup_process(process);
    }

    schedule(NULL);
}

void cleanup_process(process_t *proc) {
    if (!proc || proc->status != TERMINATED) return;

    // Remove process from process list
    if (proc == process_list) {
        process_list = process_list->next;
    }

    process_t *temp = process_list;
    while (temp->next != proc) {
        temp = temp->next;
    }

    temp->next = proc->next;
    if (proc == current_process) {
        current_process = NULL;
    }

    // Free the process stack and process structure
    // TODO: unmap all pages and heap
    free_page(get_page((uint32_t)proc->stack, 0, proc->root_page_table));
    free_page_directory(proc->root_page_table);
    kfree(proc);
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
    child->parent = parent;

    strncpy(child->process_name, parent->process_name, PROCESS_NAME_MAX_LEN);
    child->status = READY;
    child->heap_start = parent->heap_start;
    child->heap_end = parent->heap_end;
    child->brk = parent->brk;
    strncpy(child->cwd, parent->cwd, sizeof(parent->cwd));

    // Copy page directory
    child->root_page_table = clone_page_directory(parent->root_page_table);
    child->stack = (void *)USER_STACK_BASE - PROCESS_STACK_SIZE;
    memcpy(child->stack, parent->stack, PROCESS_STACK_SIZE);

    // Copy process context
    memcpy(&child->context, &parent->context, sizeof(registers_t));
    child->context.eax = 0; // Return value for child process

    uint32_t esp_offset = parent->context.esp - (uint32_t)parent->stack;
    uint32_t ebp_offset = parent->context.ebp - (uint32_t)parent->stack;
    child->context.esp = (uint32_t)child->stack + esp_offset;
    child->context.ebp = (uint32_t)child->stack + ebp_offset;

    add_process(child);
    return child->pid;
}

void user_sys_exit(int code) {
    asm volatile("int $0x80" : : "a"(5), "b"(code));
}

int exec(char *path) {
    int fd = vfs_open(path, VFS_FLAG_READ);
    if (fd < 0) {
        printf("Failed to load ELF file\n");
        return -1;
    }

    process_t *proc = get_current_process();
    page_directory_t* new_page_dir = clone_page_directory(kpage_dir);
    free_page_directory(proc->root_page_table);

    proc->root_page_table = new_page_dir;
    switch_page_directory(proc->root_page_table);

    elf_header_t* elf = load_elf(proc->fds[fd]);
    if (!elf) {
        printf("Failed to load ELF file\n");
        return -1;
    }

    proc->stack = (void *)USER_STACK_BASE - PROCESS_STACK_SIZE;
    map_memory(proc->root_page_table, proc->stack, 0, PROCESS_STACK_SIZE, 0x7);

    proc->fds[0] = vfs_get_file(0);
    proc->fds[1] = vfs_get_file(1);
    proc->fds[2] = vfs_get_file(2);

    uint32_t *stack = USER_STACK_BASE - 12;
    *--stack = 0; // Return value
    *--stack = (uint32_t)user_sys_exit;

    proc->status = READY;
    strncpy(proc->process_name, path, PROCESS_NAME_MAX_LEN);

    memset(&proc->context, 0, sizeof(registers_t));
    proc->context.eip = elf->entry;
    proc->context.esp = stack; // offset by 4 bytes for first push
    proc->context.ebp = proc->context.esp;
    proc->context.cs = USER_CS;
    proc->context.ds = USER_DS;
    proc->context.ss = USER_SS;
    proc->context.eflags = 0x202;

    kfree(elf);
    switch_context(&proc->context);
    return -1;
}
