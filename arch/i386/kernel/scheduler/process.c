#include "kernel/process.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include "kernel/elf.h"
#include "kernel/vfs.h"
#include <stdbool.h>

#define USER_STACK_TOP 0xB0000000

#define PUSH(stack, type, value) \
    stack -= sizeof(type); \
    *((type *)stack) = value;



extern page_directory_t* kpage_dir;
extern thread_t* current_thread;
extern void user_exit();
extern void switch_context(uint32_t* prev, thread_context_t* context);

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
    process_t *proc = thread->owner;
    int thread_count = 0;
    for (thread_t *t = proc->thread_list; t; t = t->next) {
        thread_count++;
    }

    void *stack_top = (void *)USER_STACK_TOP - thread_count * PROCESS_STACK_SIZE;
    void *stack_bottom = stack_top - PROCESS_STACK_SIZE;

    map_memory(proc->root_page_table, stack_bottom, 0, PROCESS_STACK_SIZE, 0x7);
    thread->user_stack = stack_bottom;
    return stack_top;
}

static void * alloc_kernel_stack(thread_t *thread) {
    void *kernel_stack = kmalloc_aligned(PROCESS_STACK_SIZE, PAGE_SIZE);
    if (!kernel_stack) {
        printf("Error: Failed to allocate kernel stack for thread %s\n", thread->thread_name);
        return NULL;
    }
    thread->kernel_stack = kernel_stack;
    return kernel_stack + PROCESS_STACK_SIZE; // Return top of the stack
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
    thread->owner = proc;
    thread->status = READY;
    
    strncpy(thread->thread_name, thread_name, PROCESS_NAME_MAX_LEN);
    thread->thread_name[PROCESS_NAME_MAX_LEN - 1] = '\0';

    // Set up thread stack for context switch
    uint32_t *stack = (uint32_t *)alloc_kernel_stack(thread);
    
    *--stack = (uint32_t)entry_point;
    printf("Creating thread: %s (entry: %x)\n", thread->thread_name, entry_point);
    for (int i = 0; i < 8; i++) *--stack = 0; // Clear registers

    thread->kernel_stack = stack;

    // Set up thread context
    memset(&thread->context, 0, sizeof(thread_context_t));
    if (!proc->is_kernel_process) {
        thread->context.user_esp = (uint32_t *)alloc_user_stack(thread);
    }

    thread->context.esp = thread->kernel_stack;
    thread->context.ebp = thread->kernel_stack;
    thread->context.eip = (uint32_t)entry_point;
    thread->context.cs = proc->is_kernel_process ? KERNEL_CS : USER_CS;
    thread->context.ss = proc->is_kernel_process ? KERNEL_DS : USER_DS;
    thread->context.eflags = 0x202;

    // Add to process's thread list
    if (!proc->thread_list) {
        proc->thread_list = thread;
    } else {
        thread_t *last_thread = proc->thread_list;
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
    strncpy(proc->process_name, process_name, PROCESS_NAME_MAX_LEN);

    proc->is_kernel_process = (flags & PROCESS_FLAG_KERNEL) != 0;
    proc->root_page_table = proc->is_kernel_process ? kpage_dir : clone_page_directory(kpage_dir);
    if (!proc->root_page_table) {
        printf("Error: Failed to create page directory for process %s\n", process_name);
        kfree(proc);
        return NULL;
    }

    proc->heap_start = USER_HEAP_START;
    proc->brk = proc->heap_start;
    proc->heap_end = proc->heap_start;

    strncpy(proc->cwd, "/home", sizeof(proc->cwd));

    proc->fds[0] = vfs_get_file(0);
    proc->fds[1] = vfs_get_file(1);
    proc->fds[2] = vfs_get_file(2);

    thread_t *main_thread = create_thread(proc, entry_point, process_name);
    proc->thread_list = main_thread;
    proc->main_thread = main_thread;
    if (!main_thread) {
        printf("Error: Failed to create main thread for process %s\n", process_name);
        free_page_directory(proc->root_page_table);
        kfree(proc);
        return NULL;
    }

    proc->status = READY;
    return proc;
}

void kill_thread(thread_t *thread) {
    if (!thread || thread->status == TERMINATED) return;
    thread->status = TERMINATED;

    remove_thread(thread);
    // if (thread->stack && !thread->owner->is_kernel_process) {
    //     if (thread->owner->is_kernel_process) {
    //         // kfree(thread->stack);
    //     } else {
    //         free_page(get_page((uint32_t)thread->stack, 0, thread->owner->root_page_table));
    //     }
    // }

    kfree(thread);
}

void kill_process_threads(process_t *proc) {
    if (!proc) return;

    thread_t *thread = proc->thread_list;
    while (thread) {
        thread_t *next = thread->next;
        kill_thread(thread);
        thread = next;
    }
    proc->thread_list = NULL;
}

void kill_process(process_t *process, int status) {
    if (!process || process->status == TERMINATED) return;
    kill_process_threads(process);
    process->status = TERMINATED;

    // wake up parent if waiting
    if (process->parent && process->parent->status == WAITING) {
        process->parent->status = READY;
        process->parent->main_thread->status = READY;
    //     thread_t *parent_thread = process->parent->threads;
    //     process->->context.eax = status; // TODO: save child status
    //     // TODO: save child status
        cleanup_process(process);
    } else {
        cleanup_process(process);
    }

    printf("Process %s (PID: %d) terminated with status %d\n", process->process_name, process->pid, status);
    schedule(NULL);
}

void cleanup_process(process_t *proc) {
    if (!proc || proc->status != TERMINATED) return;

    // // Free the process stack and process structure
    // // TODO: unmap all pages and heap
    free_page_directory(proc->root_page_table);
    kfree(proc);
}

int fork(registers_t *regs) {
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

    // TODO: Unmap the previous stack to avoid map_memory warning
    thread_t *child_thread = create_thread(child, parent->thread_list->context.eip, parent->thread_list->thread_name);
    child->main_thread = child_thread;
    child->thread_list = child_thread;

    // Copy process context
    // memcpy(&child->main_thread->context, &parent->thread_list->context, sizeof(thread_context_t));
    child_thread->context.eip = regs->eip;
    child_thread->context.user_esp = regs->esp;
    child_thread->context.ebp = regs->ebp;
    child_thread->context.eax = 0; // Return value for child process

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

    add_thread(child_thread);
    print_thread_list();
    return child->pid;
}

// TODO: Reuse old thread instead of destroying it and creating a new one
int exec(const char *path, char **argv) {
    printf("Executing process: %s\n", path);
	asm volatile ("cli");

    // 1. Open the ELF file
    process_t *proc = get_current_process();
    int fd = vfs_open(path, VFS_FLAG_READ);
    if (fd < 0) {
        printf("Failed to load ELF file\n");
        return -1;
    }
    
    // 2. Create new address space
    page_directory_t* new_page_dir = clone_page_directory(kpage_dir);
    switch_page_directory(proc->root_page_table);
    if (!new_page_dir) {
        printf("Failed to create new page directory\n");
        return -1;
    }

    // 3. Copy arguments into k_argv and k_strings (kernel memory)
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

    // 4. Clear existing threads after copying arguments
    kill_process_threads(proc);

    // 5. Switch to the new page directory and free the old one
    switch_page_directory(new_page_dir);
    free_page_directory(proc->root_page_table);
    proc->root_page_table = new_page_dir;
    proc->is_kernel_process = false; // Ensure this is a user process

    // 6. Load the ELF file into new page directory
    vfs_file_t* file = proc->fds[fd];
    elf_header_t* elf = load_elf(file, new_page_dir);
    if (!elf) {
        printf("Failed to load ELF file\n");
        kfree(k_argv);
        kfree(k_strings);
        return -1;
    }

    // 7. Create a new main thread for the process
    thread_t *thread = create_thread(proc, (void (*)(void))elf->entry, proc->process_name);
    proc->main_thread = thread;
    proc->thread_list = thread;

    // 8. Set up the user stack and copy arguments to the user stack
    uint32_t *stack_top = (uint32_t *)USER_STACK_TOP;
    char *stack_strings = (char *)(stack_top - total_len);
    char **stack_argv = (char **)(stack_strings - (sizeof(char *) * (argc + 1)));

    char *str_ptr = stack_strings;
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(k_argv[i]) + 1;
        memcpy(str_ptr, k_argv[i], len);
        stack_argv[i] = str_ptr;
        str_ptr += len;
    }
    stack_argv[argc] = NULL;

    kfree(k_argv);
    kfree(k_strings);
    stack_top = (uint32_t *)stack_argv;
    *--stack_top = (uint32_t)stack_argv;
    *--stack_top = argc;
    *--stack_top = (uint32_t)user_exit;

    // 9. Set up the thread context and file descriptors
    thread->context.user_esp = (uint32_t)stack_top;
    // thread->context.ebp = thread->context.esp;

    proc->fds[0] = vfs_get_file(0);
    proc->fds[1] = vfs_get_file(1);
    proc->fds[2] = vfs_get_file(2);

    proc->status = READY;

    // 10. Clean up the ELF header and switch to the new process
    kfree(elf);

    add_thread(thread);
    current_thread = thread;
    print_thread_list();
    switch_context(&thread->kernel_stack, &thread->context);

    // Should not return here
    return -1;
}
