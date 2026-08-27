// ApexKernel Tests/Kernel/TestRunner.h
// ApexOS -- Kernel self-test infrastructure (shared by every Tests/Kernel/*.c)

#ifndef APEX_TESTS_KERNEL_TEST_RUNNER_H
#define APEX_TESTS_KERNEL_TEST_RUNNER_H

#include "Core/Types.h"

void test_pass(const char *name);
void test_fail(const char *name, const char *reason);

#define TEST(name, cond, reason)     \
    do                               \
    {                                \
        if (cond)                    \
            test_pass(name);         \
        else                         \
            test_fail(name, reason); \
    } while (0)

void ktest_run_all(void);

#endif // APEX_TESTS_KERNEL_TEST_RUNNER_H
