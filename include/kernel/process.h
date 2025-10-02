#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/vfs.h"
#include "kernel/isr.h"
#include "kernel/paging.h"

#define PROCESS_NAME_MAX_LEN 32
#define PATH_NAME_MAX_LEN 256
#define MAX_OPEN_FILES 100

#define PROCESS_STACK_SIZE 0x1000

#define PROCESS_FLAG_USER 0x1
#define PROCESS_FLAG_KERNEL 0x2

#define MAX_SIGNALS 32

typedef enum {
    INIT,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} process_status_t;

typedef void (*signal_handler_t)(int);

typedef struct signal_info {
    uint32_t pending;     // Bitmask of pending signals
    signal_handler_t handlers[MAX_SIGNALS];
} signal_info_t;

typedef struct thread {
    size_t tid;
    uintptr_t eip;
    uintptr_t esp;
    uintptr_t ebp;
    uintptr_t user_esp;

    process_status_t status;
    char thread_name[PROCESS_NAME_MAX_LEN];
    void *kernel_stack; // ESP saved when context switching
    void *user_stack;

    struct process* owner;
    struct thread* next;
    struct thread* next_global; // For global scheduling
} thread_t;

typedef struct process {
    size_t pid;
    process_status_t status;
    char process_name[PROCESS_NAME_MAX_LEN];
    char cwd[PATH_NAME_MAX_LEN];
    
    page_directory_t *root_page_table;
    vfs_file_t* fds[MAX_OPEN_FILES];

    void *heap_start;
    void *brk;
    void *heap_end;

    thread_t* main_thread;
    thread_t* thread_list;
    bool is_kernel_process;

    struct process* parent;
    struct process* next;
    signal_info_t signals;
} process_t;

void scheduler_init();
void schedule(registers_t* context);
thread_t* pick_next_thread();

int proc_alloc_fd(process_t *proc, vfs_file_t *file);

process_t* create_process(char *process_name, void (*entry_point)(), uint32_t flags);
void add_process(process_t *process);
void kill_process(process_t *process, int status);
void cleanup_process(process_t *proc);

process_t* get_current_process();
process_t* get_process(size_t pid);

thread_t* get_current_thread();
thread_t* get_thread(size_t tid);

void add_thread(thread_t *thread);
void kill_thread(thread_t *thread);
void remove_thread(thread_t *thread);
void jmp_to_kernel_thread(thread_t *context);

thread_t* create_thread(process_t *proc, void (*entry_point)(), const char *thread_name);
void print_thread_list();


void *sbrk(process_t *proc, int incr);
int fork(registers_t *regs);
int exec(const char *path, char **args);

void signal_init(process_t *proc);
int signal_set_handler(process_t *proc, int signum, signal_handler_t handler);
int signal_send(process_t *proc, int signum);
void signal_deliver(process_t *proc);
