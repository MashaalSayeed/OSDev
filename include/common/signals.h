#pragma once

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPWR    30
#define SIGSYS    31

// Kernel signal API
typedef void (*sighandler_t)(int);

// Special handler values
#define SIG_DFL ((sighandler_t)0)   // default action
#define SIG_IGN ((sighandler_t)1)   // ignore
#define SIG_ERR ((sighandler_t)-1)  // error return

// Default actions for each signal
typedef enum {
    SIGACT_TERM,    // terminate process
    SIGACT_IGN,     // ignore
    SIGACT_CORE,    // terminate + core (treat as TERM for now)
    SIGACT_STOP,    // stop process (treat as TERM for now)
    SIGACT_CONT,    // continue if stopped
} sig_default_action_t;

static inline sig_default_action_t sig_default_action(int sig) {
    switch (sig) {
        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:  return SIGACT_IGN;
        case SIGCONT:   return SIGACT_CONT;
        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:   return SIGACT_STOP;
        default:        return SIGACT_TERM;
    }
}
typedef struct {
    sighandler_t handler;
    uint32_t     flags;
    uint32_t     mask;      // signals to block while this handler runs
} sigaction_t;