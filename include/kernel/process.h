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

typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
    uint32_t eip, cs, eflags, user_esp, ss;          // iret frame
} __attribute__((packed)) thread_context_t;

typedef struct thread {
    uint32_t tid;
    process_status_t status;
    char thread_name[PROCESS_NAME_MAX_LEN];

    uint32_t *kernel_stack; // ESP saved when context switching
    uint32_t *user_stack;
    thread_context_t context;

    struct process* owner;
    struct thread* next;
    struct thread* next_global; // For global scheduling
} thread_t;

typedef struct process {
    uint32_t pid;
    process_status_t status;
    char process_name[PROCESS_NAME_MAX_LEN];
    char cwd[256];
    
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
} process_t;

void scheduler_init();
void schedule(registers_t* context);
thread_t* pick_next_thread();

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
