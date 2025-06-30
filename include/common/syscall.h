#pragma once

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
#define SYSCALL_DUP2 14
#define SYSCALL_PIPE 15
#define SYSCALL_MKDIR 16
#define SYSCALL_RMDIR 17
#define SYSCALL_UNLINK 18
#define SYSCALL_YIELD 24
#define SYSCALL_FORK 57
#define SYSCALL_EXEC 59
#define SYSCALL_EXIT 60
#define SYSCALL_GETDENTS 78


// Standard file descriptors
#define STDIN 0
#define STDOUT 1
#define STDERR 2