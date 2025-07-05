#include "kernel/process.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include "kernel/elf.h"
#include "kernel/vfs.h"
#include <stdbool.h>

#define USER_STACK_BASE  0xB0000000

#define PUSH(stack, type, value) \
    stack -= sizeof(type); \
    *((type *)stack) = value;



extern page_directory_t* kpage_dir;
extern process_t* process_list;
extern process_t* current_process;
extern void user_exit();
extern void switch_context(registers_t* context);

extern vfs_file_t* console_fds[3];

static uint32_t pid_counter = 1;
static uint32_t tid_counter = 1;
static uint32_t allocate_pid() {
    return pid_counter++;
}

static uint32_t allocate_tid() {
    return tid_counter++;
}

static void * alloc_user_stack(thread_t *thread) {
    process_t *proc = thread->parent;
    int thread_count = 0;
    for (thread_t *t = proc->threads; t; t = t->next) {
        thread_count++;
    }

    void *stack_top = (void *)USER_STACK_BASE - thread_count * PROCESS_STACK_SIZE;
    void *stack_bottom = stack_top - PROCESS_STACK_SIZE;

    map_memory(proc->root_page_table, stack_bottom, 0, PROCESS_STACK_SIZE, 0x7);
    thread->stack = stack_bottom;
    return stack_top;
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

thread_t* create_thread(process_t *proc, void (*entry_point)(), const char *thread_name) {
    thread_t *thread = (thread_t *)kmalloc(sizeof(thread_t));
    if (!thread) {
        printf("Error: Failed to allocate memory for thread %s\n", thread_name);
        return NULL;
    }

    memset(thread, 0, sizeof(thread_t));
    thread->tid = allocate_tid();
    thread->parent = proc;
    thread->status = READY;

    strncpy(thread->thread_name, thread_name, PROCESS_NAME_MAX_LEN);
    thread->thread_name[PROCESS_NAME_MAX_LEN - 1] = '\0';

    // Set up thread context
    memset(&thread->context, 0, sizeof(registers_t));
    void *stack_top = NULL;
    if (proc->is_kernel_process) {
        thread->context.cs = KERNEL_CS;
        thread->context.ds = KERNEL_DS;
        thread->context.ss = KERNEL_DS;

        void *kernel_stack = kmalloc(PROCESS_STACK_SIZE);
        thread->stack = kernel_stack;
        stack_top = (void *)((uint32_t)kernel_stack + PROCESS_STACK_SIZE);
    } else {
        thread->context.cs = USER_CS;
        thread->context.ds = USER_DS;
        thread->context.ss = USER_SS;

        stack_top = alloc_user_stack(thread);
    }
    
    thread->context.esp = (uint32_t)stack_top;
    thread->context.ebp = (uint32_t)stack_top;
    thread->context.eip = (uint32_t)entry_point;
    thread->context.eflags = 0x202;

    // Add to process's thread list
    if (!proc->threads) {
        proc->threads = thread;
    } else {
        thread_t *last_thread = proc->threads;
        while (last_thread->next) {
            last_thread = last_thread->next;
        }
        last_thread->next = thread;
    }

    return thread;
}

process_t* create_process(char *process_name, void (*entry_point)(), uint32_t flags) {
    process_t *proc = (process_t *)kmalloc(sizeof(process_t));
    if (!proc) {
        printf("Error: Failed to allocate memory for process %s\n", process_name);
        return NULL;
    }

    memset(proc, 0, sizeof(process_t));
    proc->pid = allocate_pid();
    proc->is_kernel_process = (flags & PROCESS_FLAG_KERNEL) != 0;
    strncpy(proc->process_name, process_name, PROCESS_NAME_MAX_LEN);

    proc->root_page_table = clone_page_directory(kpage_dir);
    if (!proc->root_page_table) {
        printf("Error: Failed to create page directory for process %s\n", process_name);
        kfree(proc);
        return NULL;
    }

    // setup_stack(proc);

    proc->heap_start = USER_HEAP_START;
    proc->brk = proc->heap_start;
    proc->heap_end = proc->heap_start;

    strncpy(proc->cwd, "/home", sizeof(proc->cwd));

    proc->fds[0] = vfs_get_file(0);
    proc->fds[1] = vfs_get_file(1);
    proc->fds[2] = vfs_get_file(2);

    thread_t *main_thread = create_thread(proc, entry_point, process_name);
    proc->threads = main_thread;
    if (!main_thread) {
        printf("Error: Failed to create main thread for process %s\n", process_name);
        free_page_directory(proc->root_page_table);
        kfree(proc);
        return NULL;
    }

    proc->status = READY;
    return proc;
}

void kill_process(process_t *process, int status) {
    // if (!process || process->status == TERMINATED) return;
    // process->status = TERMINATED;

    // // wake up parent if waiting
    // if (process->parent && process->parent->status == WAITING) {
    //     process->parent->status = READY;
    //     thread_t *parent_thread = process->parent->threads;
    //     process->->context.eax = status; // TODO: save child status
    //     // TODO: save child status
    //     cleanup_process(process);
    // } else {
    //     cleanup_process(process);
    // }

    // schedule(NULL);
}

void cleanup_process(process_t *proc) {
    // if (!proc || proc->status != TERMINATED) return;

    // // Remove process from process list
    // if (proc == process_list) {
    //     process_list = process_list->next;
    // }

    // process_t *temp = process_list;
    // while (temp->next != proc) {
    //     temp = temp->next;
    // }

    // temp->next = proc->next;
    // if (proc == current_process) {
    //     current_process = NULL;
    // }

    // // Free the process stack and process structure
    // // TODO: unmap all pages and heap
    // free_page(get_page((uint32_t)proc->stack, 0, proc->root_page_table));
    // free_page_directory(proc->root_page_table);
    // kfree(proc);
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
    child->is_kernel_process = parent->is_kernel_process;
    strncpy(child->cwd, parent->cwd, sizeof(parent->cwd));

    // Copy page directory
    child->root_page_table = clone_page_directory(parent->root_page_table);
    switch_page_directory(parent->root_page_table);
    
    thread_t *child_thread = create_thread(child, parent->threads->context.eip, parent->threads->thread_name);
    // child->stack = parent->stack;

    // Copy process context
    // memcpy(&child->context, &parent->context, sizeof(registers_t));
    // child->context.eax = 0; // Return value for child process
    child_thread->context.eax = 0; // Return value for child process
    child->threads = child_thread;

    // Copy file descriptors
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (parent->fds[i]) {
            child->fds[i] = vfs_get_file(parent->fds[i]->fd); // TODO: Check if this is correct
            if (child->fds[i]) {
                child->fds[i]->ref_count++;
            }
        } else {
            child->fds[i] = NULL;
        }
    }

    // uint32_t esp_offset = parent->context.esp - (uint32_t)parent->stack;
    // uint32_t ebp_offset = parent->context.ebp - (uint32_t)parent->stack;
    // child->context.esp = (uint32_t)child->stack + esp_offset;
    // child->context.ebp = (uint32_t)child->stack + ebp_offset;

    add_thread(child_thread);
    return child->pid;
}

int exec(const char *path, char **argv) {
    process_t *proc = get_current_process();
    int fd = vfs_open(path, VFS_FLAG_READ);
    if (fd < 0) {
        printf("Failed to load ELF file\n");
        return -1;
    }
    
    // Create new address space
    page_directory_t* new_page_dir = clone_page_directory(kpage_dir);
    switch_page_directory(proc->root_page_table);
    if (!new_page_dir) {
        printf("Failed to create new page directory\n");
        return -1;
    }

    int argc = 0;
    size_t total_len = 0;
    if (argv) {
        while (argv[argc]) {
            total_len += strlen(argv[argc]) + 1;
            argc++;
        }
    }

    char **k_argv = NULL;
    char *k_strings = NULL;
    if (argc > 0) {
        k_argv = (char **)kmalloc((argc + 1) * sizeof(char *));
        k_strings = (char *)kmalloc(total_len);

        char *str_ptr = k_strings;
        for (int i = 0; i < argc; i++) {
            size_t len = strlen(argv[i]) + 1;
            memcpy(str_ptr, argv[i], len);
            k_argv[i] = str_ptr;
            str_ptr += len;
        }
        k_argv[argc] = NULL;
    }

    strncpy(proc->process_name, path, PROCESS_NAME_MAX_LEN);
    proc->process_name[PROCESS_NAME_MAX_LEN - 1] = '\0'; // Ensure null termination

    switch_page_directory(new_page_dir);
    free_page_directory(proc->root_page_table);
    proc->root_page_table = new_page_dir;

    vfs_file_t* file = proc->fds[fd];
    elf_header_t* elf = load_elf(file, new_page_dir);
    if (!elf) {
        printf("Failed to load ELF file\n");
        kfree(k_argv);
        kfree(k_strings);
        return -1;
    }

    // setup_stack(proc);
    alloc_user_stack(proc->threads);
    uint32_t *stack = (uint32_t *)(USER_STACK_BASE);
    stack = (uint32_t *)((uint32_t)stack & ~0xF);
    char *stack_strings = (char *)(stack - total_len);
    char **stack_argv = (char **)(stack_strings - (sizeof(char *) * (argc + 1)));

    // Copy strings onto the stack
    char *str_ptr = stack_strings;
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(k_argv[i]) + 1;
        memcpy(str_ptr, k_argv[i], len);
        stack_argv[i] = str_ptr;
        str_ptr += len;
    }
    stack_argv[argc] = NULL;

    // Free kernel buffer after copying
    kfree(k_argv);
    kfree(k_strings);

    stack = (uint32_t *)stack_argv;
    *--stack = (uint32_t)stack_argv;
    *--stack = argc;
    *--stack = (uint32_t)user_exit;

    proc->fds[0] = vfs_get_file(0);
    proc->fds[1] = vfs_get_file(1);
    proc->fds[2] = vfs_get_file(2);

    proc->status = READY;

    // memset(&proc->context, 0, sizeof(registers_t));
    // proc->context.eip = elf->entry;
    // proc->context.esp = (uint32_t)stack;
    // proc->context.ebp = proc->context.esp;
    // proc->context.cs = USER_CS;
    // proc->context.ds = USER_DS;
    // proc->context.ss = USER_SS;
    // proc->context.eflags = 0x202;

    kfree(elf);
    // switch_context(&proc->context);

    // Should not return here
    return -1;
}
