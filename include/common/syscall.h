#pragma once

#include <stdint.h>

// This file defines the syscall numbers for the kernel.
// I tried to keep the syscall numbers consistent with the Linux kernel.
#define SYSCALL_READ 0
#define SYSCALL_WRITE 1
#define SYSCALL_OPEN 2
#define SYSCALL_CLOSE 3
#define SYSCALL_LSEEK 8
#define SYSCALL_WAITPID 9
#define SYSCALL_GETPID 10
#define SYSCALL_SBRK 11
#define SYSCALL_GETCWD 12
#define SYSCALL_CHDIR 13
#define SYSCALL_DUP 14
#define SYSCALL_DUP2 15
#define SYSCALL_PIPE 16
#define SYSCALL_MKDIR 17
#define SYSCALL_RMDIR 18
#define SYSCALL_UNLINK 26
#define SYSCALL_YIELD 50
#define SYSCALL_FORK 57
#define SYSCALL_EXEC 59
#define SYSCALL_EXIT 60
#define SYSCALL_GETDENTS 78
#define SYSCALL_FSTAT 80
#define SYSCALL_STAT 81
#define SYSCALL_GET_TICKS 100
#define SYSCALL_NANOSLEEP 162
#define SYSCALL_MMAP 192
#define SYSCALL_MUNMAP 91
#define SYSCALL_FCNTL 72


/* --- Shared-memory & framebuffer (user-space compositor) --- */
#define SYSCALL_SHM_CREATE  19
#define SYSCALL_SHM_MAP     20
#define SYSCALL_SHM_UNMAP   21
#define SYSCALL_SHM_DESTROY 22
#define SYSCALL_FB_MAP      23

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