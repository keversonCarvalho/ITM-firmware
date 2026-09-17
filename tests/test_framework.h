#ifndef ITM_TEST_FRAMEWORK_H
#define ITM_TEST_FRAMEWORK_H

#include <stdbool.h>
#include <stdio.h>

typedef bool (*itm_test_fn_t)(void);

#define TEST_ASSERT(condition)                                                   \
    do {                                                                         \
        if (!(condition)) {                                                       \
            printf("  assertion failed: %s (%s:%d)\n", #condition, __FILE__,   \
                   __LINE__);                                                     \
            return false;                                                        \
        }                                                                        \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual) TEST_ASSERT((expected) == (actual))

int itm_run_test(const char *name, itm_test_fn_t function);

#endif
