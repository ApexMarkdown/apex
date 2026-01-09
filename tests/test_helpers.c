/**
 * Test Helper Functions Implementation
 */

#include "test_helpers.h"
#include <string.h>
#include <stdarg.h>

/* Test statistics (shared across all test files) */
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

/* When non-zero, only failing tests (and their context) are printed */
int errors_only_output = 0;

/**
 * Assert that string contains substring
 */
bool assert_contains(const char *haystack, const char *needle, const char *test_name) {
    tests_run++;

    if (strstr(haystack, needle) != NULL) {
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test_name);
        }
        return true;
    } else {
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test_name);
        printf("  Looking for: %s\n", needle);
        printf("  In:          %s\n", haystack);
        return false;
    }
}

/**
 * Assert that string does NOT contain substring
 */
bool assert_not_contains(const char *haystack, const char *needle, const char *test_name) {
    tests_run++;

    if (strstr(haystack, needle) == NULL) {
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test_name);
        }
        return true;
    } else {
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test_name);
        printf("  Should NOT contain: %s\n", needle);
        printf("  But found in:        %s\n", haystack);
        return false;
    }
}

/**
 * Assert that a boolean option is set correctly
 */
bool assert_option_bool(bool actual, bool expected, const char *test_name) {
    tests_run++;
    if (actual == expected) {
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test_name);
        }
        return true;
    } else {
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test_name);
        printf("  Expected: %s, Got: %s\n", expected ? "true" : "false", actual ? "true" : "false");
        return false;
    }
}

/**
 * Assert that a string option matches
 */
bool assert_option_string(const char *actual, const char *expected, const char *test_name) {
    tests_run++;
    if (actual && expected && strcmp(actual, expected) == 0) {
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test_name);
        }
        return true;
    } else {
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test_name);
        printf("  Expected: %s, Got: %s\n", expected ? expected : "(null)", actual ? actual : "(null)");
        return false;
    }
}

/**
 * Report a test result (for manual test cases)
 * Updates test statistics and prints output based on errors_only_output flag
 */
void test_result(bool passed, const char *test_name) {
    tests_run++;
    if (passed) {
        tests_passed++;
        if (!errors_only_output) {
            printf(COLOR_GREEN "✓" COLOR_RESET " %s\n", test_name);
        }
    } else {
        tests_failed++;
        printf(COLOR_RED "✗" COLOR_RESET " %s\n", test_name);
    }
}

/**
 * Report a test result with formatted message (for manual test cases)
 * Updates test statistics and prints output based on errors_only_output flag
 */
void test_resultf(bool passed, const char *format, ...) {
    tests_run++;
    if (passed) {
        tests_passed++;
        if (!errors_only_output) {
            va_list args;
            va_start(args, format);
            printf(COLOR_GREEN "✓" COLOR_RESET " ");
            vprintf(format, args);
            printf("\n");
            va_end(args);
        }
    } else {
        tests_failed++;
        va_list args;
        va_start(args, format);
        printf(COLOR_RED "✗" COLOR_RESET " ");
        vprintf(format, args);
        printf("\n");
        va_end(args);
    }
}
