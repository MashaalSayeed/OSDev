#pragma once

#include <stdint.h>

// This file defines the syscall numbers for the kernel.
// I tried to keep the syscall numbers consistent with the Linux kernel.
#define SYSCALL_EXIT 1
#define SYSCALL_FORK 2
#define SYSCALL_READ 3
#define SYSCALL_WRITE 4
#define SYSCALL_OPEN 5
#define SYSCALL_CLOSE 6
#define SYSCALL_WAITPID 7
#define SYSCALL_UNLINK 10
#define SYSCALL_EXEC 11
#define SYSCALL_CHDIR 12
#define SYSCALL_LSEEK 19
#define SYSCALL_GETPID 20
#define SYSCALL_MKDIR 39
#define SYSCALL_RMDIR 40
#define SYSCALL_DUP 41
#define SYSCALL_PIPE 42
#define SYSCALL_BRK 45
#define SYSCALL_SBRK 46
#define SYSCALL_YIELD 50
#define SYSCALL_FCNTL 55
#define SYSCALL_DUP2 63
#define SYSCALL_GETDENTS 78
#define SYSCALL_MUNMAP 91
#define SYSCALL_GET_TICKS 100
#define SYSCALL_STAT 106
#define SYSCALL_FSTAT 108
#define SYSCALL_MPROTECT 125
#define SYSCALL_NANOSLEEP 162
#define SYSCALL_GETCWD 183
#define SYSCALL_MMAP2 192
#define SYSCALL_EXIT_GROUP 252


/* --- Shared-memory & framebuffer (user-space compositor) --- */
#define SYSCALL_SHM_CREATE  29
#define SYSCALL_SHM_MAP     30
#define SYSCALL_SHM_UNMAP   31
#define SYSCALL_SHM_DESTROY 32
#define SYSCALL_FB_MAP      33

// Standard file descriptors
#define STDIN 0
#define STDOUT 1
#define STDERR 2

#define F_DUPFD   0
#define F_GETFD   1
#define F_SETFD   2
#define F_GETFL   3
#define F_SETFL   4
#define FD_CLOEXEC 1

#define MAP_PRIVATE    0x02  // private mapping, changes not shared
#define MAP_ANONYMOUS  0x20  // not file-backed
#define MAP_FIXED      0x10  // must use exact address given

typedef struct {
    uint32_t size;
    uint32_t mode;
    // uint32_t inode;
} stat_t;

typedef struct {
    uint32_t tv_sec;
    uint32_t tv_nsec;
} timespec_t;

typedef struct {
    int entry_number;    // -1 = allocate new
    uint32_t base_addr;
    uint32_t limit;
    unsigned seg_32bit:1;
    unsigned contents:2;
    unsigned read_exec_only:1;
    unsigned limit_in_pages:1;
    unsigned seg_not_present:1;
    unsigned useable:1;
} user_desc_t;