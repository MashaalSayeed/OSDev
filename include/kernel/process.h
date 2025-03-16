#pragma once

#include <stdint.h>
#include <stddef.h>
#include "kernel/vfs.h"
#include "kernel/isr.h"

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

// PCB (Process Control Block)
typedef struct process {
    size_t pid;
    process_status_t status;
    char process_name[PROCESS_NAME_MAX_LEN];

    struct process* parent;
    registers_t context;
    void *root_page_table;
    void *stack;

    // int priority;
    // int file_descriptors[MAX_OPEN_FILES];
    
    void *heap_start;
    void *brk;
    void *heap_end;

    char cwd[256];
    vfs_file_t* fds[MAX_OPEN_FILES];

    struct process* next;
} process_t;

void scheduler_init();
void schedule(registers_t* context);
process_t* create_process(char *process_name, void (*entry_point)(), uint32_t flags);
void add_process(process_t *process);
void kill_process(process_t *process, int status);
void cleanup_process(process_t *proc);
void print_process_list();
process_t* get_current_process();
process_t* get_process(size_t pid);
void *sbrk(process_t *proc, int incr);
int fork();
int exec(const char *path, char **args);
