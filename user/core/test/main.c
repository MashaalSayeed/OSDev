#include "test_framework.h"
#include "test_utils.h"
#include "test_fd.h"
#include "test_signals.h"
#include "test_vfs.h"
#include "test_proc.h"

int main()
{
    setup_test_files();

    test_dup_basic();
    test_dup2();
    test_fcntl();
    test_stdout_redirect();
    test_refcount();
    test_pipe_dup2();
    test_signals();
    test_waitpid_any();

    test_session_pgrp();

    test_vfs_basic();
    test_vfs_readwrite();
    test_vfs_dirs();
    test_vfs_unlink();
    test_vfs_rename();
    test_vfs_getdents();
    test_tty_ioctl_basic();
    test_path_syscalls_basic();

    test_print_summary();
    return test_get_failures() ? 1 : 0;
}
