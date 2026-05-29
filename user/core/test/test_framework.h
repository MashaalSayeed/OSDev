#pragma once

void test_pass(const char *name);
void test_fail(const char *name, const char *reason);
void test_print_summary(void);
int test_get_failures(void);

#define CHECK(name, cond, reason)            \
    do                                       \
    {                                        \
        if (cond)                            \
            test_pass(name);                 \
        else                                 \
            test_fail(name, reason);         \
    } while (0)
