#include <stdbool.h>
#include "kernel/process.h"
#include "kernel/kheap.h"
#include "kernel/paging.h"
#include "kernel/printf.h"
#include "libc/string.h"
#include "kernel/elf.h"
#include "kernel/vfs.h"
#include "kernel/signals.h"
#include "kernel/gdt.h"
#include "kernel/system.h"

#define PUSH(stack, type, value) \
    stack -= sizeof(type); \
    *((type *)stack) = value;



extern page_directory_t* kpage_dir;
extern uint32_t kpage_dir_phys;
extern thread_t* current_thread;
extern void switch_context(thread_t* context);
extern void switch_task(uintptr_t* prev, uintptr_t next);
extern struct tss_entry tss_entry;

extern uintptr_t read_eip();

extern vfs_file_t* console_fds[3];

process_t *process_list = NULL;

static uint32_t pid_counter = 1;
static uint32_t tid_counter = 1;
static uint32_t allocate_pid() {
    return pid_counter++;
}

static uint32_t allocate_tid() {
    return tid_counter++;
}

static void add_process(process_t *process) {
    if (!process_list) {
        process_list = process;
    } else {
        process_t *temp = process_list;
        while (temp->next) {
            temp = temp->next;
        }
        temp->next = process;
    }
}

static void remove_process(process_t *process) {
    if (!process_list || !process) return;

    if (process_list == process) {
        process_list = process->next;
        return;
    }

    process_t *temp = process_list;
    while (temp->next && temp->next != process) {
        temp = temp->next;
    }
    if (temp->next == process) {
        temp->next = process->next;
    }
}

static void * alloc_user_stack(thread_t *thread) {
    process_t *proc = thread->owner;
    int thread_count = 0;
    for (thread_t *t = proc->thread_list; t; t = t->next) {
        thread_count++;
    }
    
    void *stack_top = (void *)USER_STACK_TOP - thread_count * PROCESS_STACK_SIZE;
    void *stack_bottom = (void *)(stack_top - PROCESS_STACK_SIZE);

    map_memory(proc->root_page_table, (uint32_t)stack_bottom, -1, PROCESS_STACK_SIZE, 0x7);
    thread->user_stack = stack_bottom;
    return stack_top;
}

static void * alloc_kernel_stack(thread_t *thread) {
    void *kernel_stack = kmalloc_aligned(PROCESS_STACK_SIZE, PAGE_SIZE);
    if (!kernel_stack) {
        printf("Error: Failed to allocate kernel stack for thread %s\n", thread->thread_name);
        return 0;
    }
    thread->kernel_stack = kernel_stack;
    return kernel_stack + PROCESS_STACK_SIZE; // Return top of the stack
}

process_t* get_process(size_t pid) {
    process_t *temp = process_list;
    while (temp) {
        if (temp->pid == pid) return temp;
        temp = temp->next;
    }
    return NULL;
}

process_t* find_child_process(process_t *parent) {
    if (!parent) return NULL;
    process_t *temp = process_list;
    while (temp) {
        if (temp->parent == parent) return temp;
        temp = temp->next;
    }
    return NULL;
}

process_t* find_zombie_child(process_t *parent) {
    if (!parent) return NULL;
    process_t *temp = process_list;
    while (temp) {
        if (temp->parent == parent && temp->status == ZOMBIE) return temp;
        temp = temp->next;
    }
    return NULL;
}

int proc_alloc_fd(process_t *proc, vfs_file_t *file) {
    for (int fd = 3; fd < MAX_OPEN_FILES; fd++) {
        if (proc->fds[fd] == NULL) {
            proc->fds[fd] = file;
            file->fd = fd;
            return fd;
        }
    }
    return -1; // No available file descriptor
}

int proc_close_fd(process_t *proc, int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !proc->fds[fd]) return -1;
    vfs_file_t *file = proc->fds[fd];

    proc->fds[fd] = NULL;
    return vfs_close(file);
}

void* sbrk(process_t *proc, int increment) {
    if (increment == 0) return proc->brk;

    void *old_brk = proc->brk;
    void *new_brk = proc->brk + increment;

    if (increment > 0) {
        // Expand heap by allocating pages
        while (proc->brk < new_brk) {
            alloc_page(get_page((uintptr_t)proc->brk, 1, proc->root_page_table), 0x7);
            proc->brk += PAGE_SIZE;
        }
    } 
    else if (increment < 0) {
        // Shrink heap (optional)
        while (proc->brk > new_brk) {
            free_page(get_page((uintptr_t)proc->brk - PAGE_SIZE, 0, proc->root_page_table));
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
    void *stack = alloc_kernel_stack(thread);

    PUSH(stack, uint32_t, (uintptr_t)entry_point);
    for (int i = 0; i < 8; i++) {
        PUSH(stack, uint32_t, 0); // Clear registers
    }
    PUSH(stack, uint32_t, 0x23); // gs

    // Set up thread context
    if (!proc->is_kernel_process) {
        thread->user_esp = (uintptr_t)alloc_user_stack(thread);
    }

    thread->esp = (uintptr_t)stack;
    thread->ebp = (uintptr_t)stack;
    thread->eip = (uintptr_t)entry_point;

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
    proc->mmap_base = (void *)MMAP_BASE; 
    if (!proc->root_page_table) {
        printf("Error: Failed to create page directory for process %s\n", process_name);
        kfree(proc);
        return NULL;
    }

    proc->heap_start = (void *)USER_HEAP_START;
    proc->brk = proc->heap_start;

    strncpy(proc->cwd, "/home", sizeof(proc->cwd));

    proc->pending_signals = 0;
    proc->signal_mask = 0;
    memset(proc->signal_handlers, 0, sizeof(proc->signal_handlers));
    map_signal_trampoline(proc);

    proc->fds[0] = vfs_get_file(0);
    proc->fds[1] = vfs_get_file(1);
    proc->fds[2] = vfs_get_file(2);

    thread_t *main_thread = create_thread(proc, entry_point, process_name);
    proc->main_thread = main_thread;
    if (!main_thread) {
        printf("Error: Failed to create main thread for process %s\n", process_name);
        free_page_directory(proc->root_page_table);
        kfree(proc);
        return NULL;
    }

    proc->status = READY;

    wait_queue_init(&proc->wait_queue);
    add_process(proc);
    return proc;
}

void thread_wake(thread_t *thread) {
    if (!thread || thread->status != WAITING) return;
    thread->status = READY;
}

void kill_thread(thread_t *thread) {
    if (!thread || thread->status == TERMINATED) return;
    thread->status = TERMINATED;

    remove_thread(thread);

    if (thread->user_stack) {
        uint32_t stack_bottom = (uint32_t)thread->user_stack;
        uint32_t stack_top = stack_bottom + PROCESS_STACK_SIZE;
        for (uint32_t addr = stack_bottom; addr < stack_top; addr += PAGE_SIZE) {
            free_page(get_page(addr, 0, thread->owner->root_page_table));
        }
    }

    if (thread == current_thread) return;
    if (thread->kernel_stack) {
        kfree_aligned((void *)thread->kernel_stack);
        kfree(thread);
    }
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
    proc->main_thread = NULL;
}

void kill_process(process_t *process, int status) {
    if (!process || process->status == TERMINATED || process->status == ZOMBIE) return;
    printf("Process %s (PID: %d) terminating with status %d\n", process->process_name, process->pid, status);

    kill_process_threads(process);

    process->status = ZOMBIE;
    process->exit_code = status;

    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        if (process->fds[i]) {
            proc_close_fd(process, i);
        }
    }

    // wake up parent if waiting
    if (!wait_queue_empty(&process->wait_queue)) {
        wait_queue_wake_all(&process->wait_queue);
    }

    if (process->parent) {
        process_t *parent = process->parent;
        if (parent->signal_handlers[SIGCHLD].handler != SIG_IGN) {
            parent->pending_signals |= (1u << SIGCHLD);
        }
        wait_queue_wake_all(&parent->wait_queue);
    } else {
        cleanup_process(process);
    }

    schedule(NULL);
}

void cleanup_process(process_t *proc) {
    if (!proc || proc->status != ZOMBIE) return;
    kprintf(DEBUG, "cleanup_process: pid=%d dir=%x\n", proc->pid, proc->root_page_table);

    // // Free the process stack and process structure
    // // TODO: unmap all pages and heap
    free_page_directory(proc->root_page_table);
    remove_process(proc);
    kfree(proc);
}

extern void fork_trampoline(void);
int fork(registers_t *regs) {
    asm volatile ("cli");
    
    process_t *parent = get_current_process();
    thread_t *parent_thread = parent->main_thread;

    // Allocate and initialize child process
    process_t *child = (process_t *)kmalloc(sizeof(process_t));
    if (!child) {
        printf("Error: Failed to allocate memory for child process\n");
        return -1;
    }

    memset(child, 0, sizeof(process_t));
    child->pid = allocate_pid();
    child->parent = parent;
    child->status = READY;
    child->heap_start = parent->heap_start;
    child->brk = parent->brk;
    child->mmap_base = parent->mmap_base;
    child->is_kernel_process = parent->is_kernel_process;
    child->pending_signals = 0;
    child->signal_mask = 0;
    // memset(child->signal_handlers, 0, sizeof(child->signal_handlers));
    memcpy(child->signal_handlers, parent->signal_handlers, sizeof(parent->signal_handlers));
    strncpy(child->process_name, parent->process_name, PROCESS_NAME_MAX_LEN);
    strncpy(child->cwd, parent->cwd, sizeof(parent->cwd));

    // Clone page directory
    child->root_page_table = clone_page_directory(parent->root_page_table);
    if (!child->root_page_table) {
        kprintf(ERROR, "fork: failed to clone page directory\n");
        kfree(child);
        return -1;
    }

    // Copy file descriptors (increment ref counts)
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        child->fds[i] = parent->fds[i];
        if (child->fds[i]) {
            child->fds[i]->ref_count++;
        }
    }

    // Initialize child wait queue
    wait_queue_init(&child->wait_queue);
    map_signal_trampoline(child); 
    add_process(child);

    // Step 9: Allocate child thread
    thread_t *child_thread = (thread_t *)kmalloc(sizeof(thread_t));
    if (!child_thread) {
        kprintf(ERROR, "fork: failed to allocate child thread\n");
        free_page_directory(child->root_page_table);
        kfree(child);
        return -1;
    }

    memset(child_thread, 0, sizeof(thread_t));
    child_thread->tid = allocate_tid();
    child_thread->owner = child;
    child_thread->status = READY;
    child_thread->next = NULL;
    child_thread->next_global = NULL;
    child->main_thread = child_thread;
    child->thread_list = child_thread;
    strncpy(child_thread->thread_name, "", PROCESS_NAME_MAX_LEN);


    // Step 10: Allocate kernel stack for child and copy parent's kernel stack
    void *child_kernel_stack = alloc_kernel_stack(child_thread);
    if (!child_kernel_stack) {
        kprintf(ERROR, "fork: failed to allocate kernel stack for child thread\n");
        kfree(child_thread);
        free_page_directory(child->root_page_table);
        kfree(child);
        return -1;
    }

    // Copy parent's entire kernel stack to child
    memcpy(child_thread->kernel_stack, parent_thread->kernel_stack, PROCESS_STACK_SIZE);

    // Step 11: Fix stack pointers in child's kernel stack
    uintptr_t offset = (uintptr_t)child_thread->kernel_stack - (uintptr_t)parent_thread->kernel_stack;
    uintptr_t *sp = (uintptr_t *)((uintptr_t)regs + offset);
    
    *(--sp) = (uintptr_t)fork_trampoline;  // ret addr
    *(--sp) = parent_thread->gs;     // matches your switch_task
    *(--sp) = regs->edi;
    *(--sp) = regs->esi;
    *(--sp) = regs->ebp;
    *(--sp) = 0;            // esp dummy for popad
    *(--sp) = regs->ebx;
    *(--sp) = regs->edx;
    *(--sp) = regs->ecx;
    *(--sp) = regs->eax;    // eax (will be overwritten anyway)

    child_thread->esp = (uintptr_t)sp;
    child_thread->ebp = parent_thread->ebp + offset;
    
    registers_t *child_regs = (registers_t *)((uintptr_t)regs + offset);
    child_regs->eax = 0;           // child sees fork() == 0
    regs->eax = child->pid;

    // Step 14-15: Attach child thread to PCB and add to scheduler
    add_thread(child_thread);
    asm volatile ("sti");
    return child->pid;
}

// TODO: Reuse old thread instead of destroying it and creating a new one
#define AT_NULL   0
#define AT_PAGESZ 6
// Returns the final user esp, or 0 on failure
static uint32_t build_user_stack(page_directory_t *dir, char **argv, int argc, char **envp, int envc) {
    uint32_t u_esp = USER_STACK_TOP;
    uint32_t zero  = 0;

    // ── 1. String data ────────────────────────────────────────────────────────
    uint32_t u_strings = 0;
    uint32_t *argv_addrs = NULL;
    uint32_t *env_addrs = NULL;
    size_t total_len = 0;

    for (int i = 0; i < argc; i++) total_len += strlen(argv[i]) + 1;
    for (int i = 0; i < envc; i++) total_len += strlen(envp[i]) + 1;

    if (total_len > 0) {
        char *k_strings = kmalloc(total_len);
        if (!k_strings) return 0;

        if (argc > 0) {
            argv_addrs = kmalloc(argc * sizeof(uint32_t));
            if (!argv_addrs) {
                kfree(k_strings);
                return 0;
            }
        }
        if (envc > 0) {
            env_addrs = kmalloc(envc * sizeof(uint32_t));
            if (!env_addrs) {
                if (argv_addrs) kfree(argv_addrs);
                kfree(k_strings);
                return 0;
            }
        }

        char *ptr = k_strings;
        uint32_t off = 0;
        for (int i = 0; i < argc; i++) {
            size_t len = strlen(argv[i]) + 1;
            memcpy(ptr, argv[i], len);
            argv_addrs[i] = off;
            ptr += len;
            off += len;
        }
        for (int i = 0; i < envc; i++) {
            size_t len = strlen(envp[i]) + 1;
            memcpy(ptr, envp[i], len);
            env_addrs[i] = off;
            ptr += len;
            off += len;
        }

        u_esp    -= total_len;
        u_esp    &= ~0xF;
        u_strings = u_esp;
        copy_to_user(dir, u_strings, k_strings, total_len);
        kfree(k_strings);

        for (int i = 0; i < argc; i++) argv_addrs[i] = u_strings + argv_addrs[i];
        for (int i = 0; i < envc; i++) env_addrs[i] = u_strings + env_addrs[i];
    }

    // ── 2. Aux vector ─────────────────────────────────────────────────────────
    // Must come before envp/argv in memory (higher address) since
    // _start scans upward past envp NULL to find auxv
    uint32_t auxv[] = { AT_PAGESZ, PAGE_SIZE, AT_NULL, 0 };
    u_esp -= sizeof(auxv);
    u_esp &= ~0xF;
    copy_to_user(dir, u_esp, auxv, sizeof(auxv));

    // ── 3. envp NULL terminator ─────────────────────────────────────────────
    u_esp -= 4;
    copy_to_user(dir, u_esp, &zero, 4);

    // ── 4. envp[0..envc-1] — push in reverse ─────────────────────────────────
    for (int i = envc - 1; i >= 0; i--) {
        u_esp -= 4;
        uint32_t uptr = env_addrs[i];
        copy_to_user(dir, u_esp, &uptr, 4);
    }

    // ── 5. argv NULL terminator ─────────────────────────────────────────────
    u_esp -= 4;
    copy_to_user(dir, u_esp, &zero, 4);

    // ── 6. argv[0..argc-1] — push in reverse ────────────────────────────────
    for (int i = argc - 1; i >= 0; i--) {
        u_esp -= 4;
        uint32_t uptr = argv_addrs[i];
        copy_to_user(dir, u_esp, &uptr, 4);
    }
    if (argv_addrs) kfree(argv_addrs);
    if (env_addrs) kfree(env_addrs);

    // ── 7. argc ───────────────────────────────────────────────────────────────
    // u_esp &= ~0xF;
    u_esp -= 4;
    copy_to_user(dir, u_esp, &argc, 4);

    if (u_esp < (uint32_t)(USER_STACK_TOP - PROCESS_STACK_SIZE)) {
        kprintf(ERROR, "build_user_stack: overflow\n");
        return 0;
    }

    return u_esp;
}

int exec(const char *path, char **argv, char **envp) {
    asm volatile ("cli");

    process_t        *proc        = get_current_process();
    vfs_file_t       *file        = NULL;
    page_directory_t *new_page_dir = NULL;
    elf_header_t     *elf         = NULL;
    int               ret         = -1;

    switch_page_directory(kpage_dir);

    // ── 1. Open ELF ──────────────────────────────────────────────────────────
    file = vfs_open(path, VFS_FLAG_READ);
    if (!file) {
        kprintf(ERROR, "exec: failed to open %s\n", path);
        goto fail;
    }

    // ── 2. Count argv/envp (already kernel pointers from sys_exec) ───────────
    int argc = 0;
    int envc = 0;
    if (argv) {
        while (argv[argc]) {
            argc++;
        }
    }
    if (envp) {
        while (envp[envc]) {
            envc++;
        }
    }

    // ── 3. New address space ──────────────────────────────────────────────────
    new_page_dir = clone_page_directory(kpage_dir);
    if (!new_page_dir) {
        kprintf(ERROR, "exec: failed to clone page directory\n");
        goto fail;
    }

    // ── 4. Load ELF into new address space ────────────────────────────────────
    elf = load_elf(file, new_page_dir);
    vfs_close(file);
    file = NULL;
    if (!elf) {
        kprintf(ERROR, "exec: failed to load ELF %s\n", path);
        goto fail;
    }

    // ── 5. Point of no return — save old stack, tear down old state ───────────
    strncpy(proc->process_name, path, PROCESS_NAME_MAX_LEN);
    proc->process_name[PROCESS_NAME_MAX_LEN - 1] = '\0';

    thread_t *old_thread = current_thread;
    void     *old_stack  = old_thread->kernel_stack;
    old_thread->kernel_stack = NULL;   // prevent kill_thread from freeing it

    kill_process_threads(proc);
    proc->is_kernel_process = false;
    proc->pending_signals = 0;
    proc->signal_mask = 0;
    memset(proc->signal_handlers, 0, sizeof(proc->signal_handlers));

    free_page_directory(proc->root_page_table);
    proc->root_page_table   = new_page_dir;
    new_page_dir = NULL;

    map_signal_trampoline(proc);

    // ── 6. Create new main thread (maps user stack too) ───────────────────────
    thread_t *thread = create_thread(proc, (void (*)(void))elf->entry,
                                     proc->process_name);
    if (!thread) {
        kprintf(ERROR, "exec: failed to create thread\n");
        goto fail;
    }
    proc->main_thread = thread;
    proc->thread_list = thread;

    // ── 7. Build user stack ───────────────────────────────────────────────────
    // Layout (high → low):
    //   [ string data        ]
    //   [ auxv: AT_PAGESZ,4096, AT_NULL,0 ]
    //   [ NULL               ]  ← end of envp
    //   [ envp[0..m-1] ptrs  ]
    //   [ NULL               ]  ← end of argv
    //   [ argv[0..n-1] ptrs  ]
    //   [ argc               ]  ← esp on entry to _start

    uint32_t u_esp = build_user_stack(proc->root_page_table, argv, argc, envp, envc);
    if (!u_esp) {
        kprintf(ERROR, "exec: failed to build user stack\n");
        goto fail;
    }

    // ── 9. Launch ─────────────────────────────────────────────────────────────
    thread->user_esp = u_esp;
    proc->status     = READY;
    kfree(elf);
    elf = NULL;

    add_thread(thread);
    current_thread = thread;
    tss_entry.esp0 = (uint32_t)thread->kernel_stack + PROCESS_STACK_SIZE;
    switch_page_directory(proc->root_page_table);

    // Free old thread and kernel stack — last thing before iret
    kfree(old_thread);
    kfree_aligned(old_stack);
    switch_context(thread);   // iret — never returns
    return -1;

fail:
    if (file)         vfs_close(file);
    if (elf)          kfree(elf);
    if (new_page_dir) free_page_directory(new_page_dir);
    return ret;
}


// Layout (grows downward from USER_STACK_TOP):
    //   [argc][argv ptr][envp=NULL]  ← stack_top (user esp)
    //   [argv[0] ptr][argv[1] ptr]...[NULL]
    //   [string data]
//     uint32_t u_stack_top   = USER_STACK_TOP;
//     uint32_t u_strings     = u_stack_top - total_len;
//     uint32_t u_argv_ptrs   = u_strings - (sizeof(char *) * (argc + 1));

//     // Copy string data into user stack
//     if (argc > 0) {
//         copy_to_user(proc->root_page_table, u_strings, k_strings, total_len);

//         // Build argv pointer array in kernel, fixing up user-space addresses
//         char **tmp_argv = kmalloc((argc + 1) * sizeof(char *));
//         uint32_t str_offset = 0;
//         for (int i = 0; i < argc; i++) {
//             tmp_argv[i] = (char *)(u_strings + str_offset);
//             str_offset += strlen(k_argv[i]) + 1;
//         }
//         tmp_argv[argc] = NULL;

//         copy_to_user(proc->root_page_table, u_argv_ptrs,
//                      tmp_argv, (argc + 1) * sizeof(char *));
//         kfree(tmp_argv);
//     }

//     kfree(k_argv);    k_argv    = NULL;
//     kfree(k_strings); k_strings = NULL;

//     // Push argc, argv, envp onto user stack
//     uint32_t u_esp = u_argv_ptrs;
//     uint32_t envp  = 0;
//     uint32_t argv_ptr = u_argv_ptrs;

//     u_esp -= sizeof(uint32_t); copy_to_user(proc->root_page_table, u_esp, &envp,     sizeof(uint32_t));
//     u_esp -= sizeof(uint32_t); copy_to_user(proc->root_page_table, u_esp, &argv_ptr, sizeof(uint32_t));
//     u_esp -= sizeof(uint32_t); copy_to_user(proc->root_page_table, u_esp, &argc,     sizeof(uint32_t));

//     // Verify we haven't gone below the mapped region
// if (u_esp < (uint32_t)(USER_STACK_TOP - PROCESS_STACK_SIZE)) {
//     kprintf(ERROR, "exec: argv too large, overflows user stack\n");
//     goto fail;
// }