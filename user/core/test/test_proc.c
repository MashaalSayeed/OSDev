#include "user/syscall.h"
#include "user/stdio.h"
#include "test_framework.h"

void test_session_pgrp(void)
{
    printf("\n[session/pgrp]\n");

    // int pid = syscall_getpid();
    // int pgrp = syscall_getpgrp();
    // CHECK("getpgrp returns pid", pgrp == pid, "unexpected pgrp");

    // int sid = syscall_setsid();
    // CHECK("setsid returns pid", sid == pid, "unexpected sid");

    // pgrp = syscall_getpgrp();
    // CHECK("getpgrp after setsid", pgrp == pid, "pgrp changed unexpectedly");
}
