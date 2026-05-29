#include "user/stdio.h"
#include "test_framework.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

void test_pass(const char *name)
{
    printf("  [PASS] %s\n", name);
    tests_passed++;
    tests_run++;
}

void test_fail(const char *name, const char *reason)
{
    printf("  [FAIL] %s - %s\n", name, reason);
    tests_failed++;
    tests_run++;
}

void test_print_summary(void)
{
    printf("\n============================\n");
    printf("  %d/%d passed", tests_passed, tests_run);
    if (tests_failed)
        printf(", %d FAILED", tests_failed);
    printf("\n============================\n");
}

int test_get_failures(void)
{
    return tests_failed;
}
