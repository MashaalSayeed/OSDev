#include "user/syscall.h"
#include "user/stdio.h"
#include "common/signals.h"
#include "test_framework.h"

static volatile int sig_received = 0;
static volatile int sig_number = 0;
static volatile int sigchld_seen = 0;

sighandler_t signal(int sig, sighandler_t handler)
{
    return (sighandler_t)syscall_signal(sig, (uint32_t)handler);
}

int raise(int sig)
{
    return syscall_kill(syscall_getpid(), sig);
}

static void sigusr1_handler(int sig)
{
    sig_received = 1;
    sig_number = sig;
}

static void sigchld_handler(int sig)
{
    (void)sig;
    sigchld_seen = 1;
}

void test_signals(void)
{
    printf("\n[signals]\n");

    sig_received = 0;
    signal(SIGUSR1, sigusr1_handler);
    syscall_kill(syscall_getpid(), SIGUSR1);

    for (volatile int i = 0; i < 100000 && !sig_received; i++)
    {
        syscall_yield();
    }

    CHECK("SIGUSR1 self-delivery", sig_received == 1, "handler not called");
    CHECK("SIGUSR1 correct signum", sig_number == SIGUSR1, "wrong signal number");

    sig_received = 0;
    signal(SIGUSR1, SIG_IGN);
    syscall_kill(syscall_getpid(), SIGUSR1);
    for (volatile int i = 0; i < 100000; i++)
        ;
    CHECK("SIG_IGN suppresses", sig_received == 0, "handler called despite SIG_IGN");

    signal(SIGUSR1, SIG_DFL);
    int pid = syscall_fork();
    if (pid == 0)
    {
        signal(SIGUSR1, SIG_DFL);
        for (volatile int i = 0; i < 10000000; i++)
        {
            syscall_yield();
        }
        syscall_exit(0);
    }
    else
    {
        syscall_kill(pid, SIGUSR1);
        int status = 0;
        syscall_waitpid(pid, &status, 0);
        CHECK("SIG_DFL terminates process", status == SIGUSR1, "wrong exit status");
    }

    sig_received = 0;
    signal(SIGUSR1, sigusr1_handler);
    pid = syscall_fork();
    if (pid == 0)
    {
        syscall_kill(syscall_getpid(), SIGUSR1);
        for (volatile int i = 0; i < 100000 && !sig_received; i++)
        {
            syscall_yield();
        }
        syscall_exit(sig_received ? 42 : 1);
    }
    else
    {
        int status = 0;
        syscall_waitpid(pid, &status, 0);
        CHECK("signal handler inherited by fork", status == 42, "child handler not called");
    }

    pid = syscall_fork();
    if (pid == 0)
    {
        sig_received = 0;
        signal(SIGUSR2, sigusr1_handler);

        syscall_kill(syscall_getppid(), SIGUSR1);
        int got_sig = 0;
        for (volatile int i = 0; i < 10000000; i++)
        {
            syscall_yield();
            if (sig_received)
            {
                got_sig = 1;
                break;
            }
        }
        syscall_exit(got_sig ? 0 : 1);
    }
    else
    {
        sig_received = 0;
        signal(SIGUSR1, sigusr1_handler);
        int ready = 0;
        for (volatile int i = 0; i < 100000; i++)
        {
            syscall_yield();
            if (sig_received)
            {
                ready = 1;
                break;
            }
        }
        signal(SIGUSR1, SIG_DFL);
        if (ready)
        {
            syscall_kill(pid, SIGUSR2);
        }
        int status = 0;
        syscall_waitpid(pid, &status, 0);
        CHECK("SIGUSR1 child->parent ready", ready == 1, "parent never got SIGUSR1");
        CHECK("SIGUSR2 parent->child delivery", status == 0, "child did not receive signal");
    }

    signal(SIGUSR1, SIG_DFL);
    signal(SIGUSR2, SIG_DFL);
}

void test_waitpid_any(void)
{
    printf("\n[waitpid(-1)]\n");

    sigchld_seen = 0;
    signal(SIGCHLD, sigchld_handler);

    int pid1 = syscall_fork();
    if (pid1 == 0)
    {
        syscall_exit(11);
    }

    int pid2 = syscall_fork();
    if (pid2 == 0)
    {
        syscall_exit(22);
    }

    int status1 = 0;
    int status2 = 0;
    int got1 = syscall_waitpid(-1, &status1, 0);
    int got2 = syscall_waitpid(-1, &status2, 0);

    int ok_pid1 = (got1 == pid1 || got2 == pid1);
    int ok_pid2 = (got1 == pid2 || got2 == pid2);

    int ok_status1 = ((got1 == pid1 && status1 == 11) || (got2 == pid1 && status2 == 11));
    int ok_status2 = ((got1 == pid2 && status1 == 22) || (got2 == pid2 && status2 == 22));

    CHECK("waitpid(-1) reaps pid1", ok_pid1, "pid1 not reaped");
    CHECK("waitpid(-1) reaps pid2", ok_pid2, "pid2 not reaped");
    CHECK("waitpid(-1) status pid1", ok_status1, "wrong status for pid1");
    CHECK("waitpid(-1) status pid2", ok_status2, "wrong status for pid2");

    for (volatile int i = 0; i < 100000 && !sigchld_seen; i++)
    {
        syscall_yield();
    }
    CHECK("SIGCHLD delivered", sigchld_seen != 0, "SIGCHLD not observed");

    signal(SIGCHLD, SIG_DFL);
}
