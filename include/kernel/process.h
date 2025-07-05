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

#define PROCESS_STACK_SIZE 4096

#define PROCESS_FLAG_USER 0x1
#define PROCESS_FLAG_KERNEL 0x2

typedef enum {
    INIT,
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} process_status_t;

typedef struct thread {
    uint32_t tid;
    process_status_t status;
    registers_t context;
    char thread_name[PROCESS_NAME_MAX_LEN];
    void *stack;

    struct process* parent;

    struct thread* next;
    struct thread* next_global; // For global scheduling
} thread_t;

typedef struct process {
    uint32_t pid;
    process_status_t status;
    char process_name[PROCESS_NAME_MAX_LEN];
    thread_t* threads;
    thread_t* current_thread;
    bool is_kernel_process;

    struct process* parent;
    page_directory_t *root_page_table;
    
    void *heap_start;
    void *brk;
    void *heap_end;

    char cwd[256];
    vfs_file_t* fds[MAX_OPEN_FILES];

    struct process* next;
} process_t;

void scheduler_init();
void schedule(registers_t* context);
thread_t* pick_next_thread(thread_t *current_thread);

process_t* create_process(char *process_name, void (*entry_point)(), uint32_t flags);
void add_process(process_t *process);
void kill_process(process_t *process, int status);
void cleanup_process(process_t *proc);
process_t* get_current_process();
process_t* get_process(size_t pid);


void add_thread(thread_t *thread);
thread_t* create_thread(process_t *proc, void (*entry_point)(), const char *thread_name);
void print_thread_list();


void *sbrk(process_t *proc, int incr);
int fork();
int exec(const char *path, char **args);
