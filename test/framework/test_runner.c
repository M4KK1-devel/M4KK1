/*
 * M4KK1 4P1 - test_runner.c
 * Description: Test framework runner for M4KK1.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../../sys/src/include/console.h"
#include "../../sys/src/include/memory.h"
#include "../../sys/src/include/process.h"
#include "../../sys/src/include/kernel.h"

/* Test result structure */
typedef struct {
    const char *test_name;
    bool passed;
    const char *message;
    uint32_t execution_time;
} test_result_t;

/* Test function type */
typedef bool (*test_function_t)(void);

/* Test case structure */
typedef struct test_case {
    const char *name;
    test_function_t function;
    struct test_case *next;
} test_case_t;

/* Global test list */
static test_case_t *test_list = NULL;
static uint32_t total_tests = 0;
static uint32_t passed_tests = 0;
static uint32_t failed_tests = 0;

/* Test result buffer */
#define MAX_TEST_RESULTS 256
static test_result_t test_results[MAX_TEST_RESULTS];
static uint32_t result_count = 0;

/* Add test case */
void musr_test_add_case(const char *name, test_function_t function)
{
    test_case_t *test = (test_case_t *)kmalloc(sizeof(test_case_t));
    if (!test) {
        console_write("Failed to allocate memory for test case\n");
        return;
    }

    test->name = name;
    test->function = function;
    test->next = test_list;
    test_list = test;
    total_tests++;

    console_write("Test case added: ");
    console_write(name);
    console_write("\n");
}

/* Run single test */
static bool run_single_test(test_case_t *test, test_result_t *result)
{
    uint32_t start_time, end_time;

    console_write("Running test: ");
    console_write(test->name);
    console_write("... ");

    start_time = 0;

    bool passed = false;
    const char *message = NULL;

    if (test->function) {
        passed = test->function();
        if (passed) {
            message = "PASSED";
        } else {
            message = "FAILED";
        }
    } else {
        passed = false;
        message = "NO FUNCTION";
    }

    end_time = 0;
    uint32_t execution_time = end_time - start_time;

    if (result) {
        result->test_name = test->name;
        result->passed = passed;
        result->message = message;
        result->execution_time = execution_time;
    }

    if (passed) {
        passed_tests++;
        console_write("PASSED");
    } else {
        failed_tests++;
        console_write("FAILED");
    }

    console_write(" (");
    console_write_dec(execution_time);
    console_write("ms)\n");

    return passed;
}

/* Run all tests */
void musr_test_run_all(void)
{
    test_case_t *test = test_list;
    uint32_t test_number = 1;

    console_write("\n");
    console_write("=====================================\n");
    console_write("    M4KK1 Test Framework\n");
    console_write("=====================================\n");
    console_write("\n");

    passed_tests = 0;
    failed_tests = 0;
    result_count = 0;

    while (test && result_count < MAX_TEST_RESULTS) {
        console_write("[");
        console_write_dec(test_number++);
        console_write("] ");

        run_single_test(test, &test_results[result_count]);
        result_count++;

        test = test->next;
    }

    console_write("\n");
    console_write("=====================================\n");
    console_write("Test Summary:\n");
    console_write("  Total: ");
    console_write_dec(total_tests);
    console_write("\n");
    console_write("  Passed: ");
    console_write_dec(passed_tests);
    console_write("\n");
    console_write("  Failed: ");
    console_write_dec(failed_tests);
    console_write("\n");

    if (failed_tests == 0) {
        console_write("  Result: ALL TESTS PASSED\n");
    } else {
        console_write("  Result: SOME TESTS FAILED\n");
    }

    console_write("=====================================\n");
}

/* Get test statistics */
void musr_test_get_stats(uint32_t *total, uint32_t *passed, uint32_t *failed)
{
    if (total) *total = total_tests;
    if (passed) *passed = passed_tests;
    if (failed) *failed = failed_tests;
}

/* Print detailed test results */
void musr_test_print_results(void)
{
    console_write("\nDetailed Test Results:\n");
    console_write("=====================================\n");

    for (uint32_t i = 0; i < result_count; i++) {
        console_write("[");
        console_write_dec(i + 1);
        console_write("] ");
        console_write(test_results[i].test_name);
        console_write(" - ");
        console_write(test_results[i].message);
        console_write(" (");
        console_write_dec(test_results[i].execution_time);
        console_write("ms)\n");
    }

    console_write("=====================================\n");
}

/* Memory allocation test */
static bool test_memory_allocation(void)
{
    void *ptr1 = kmalloc(1024);
    void *ptr2 = kmalloc(512);
    void *ptr3 = kmalloc(256);

    if (!ptr1 || !ptr2 || !ptr3) {
        return false;
    }

    memset(ptr1, 0xAA, 1024);
    memset(ptr2, 0xBB, 512);
    memset(ptr3, 0xCC, 256);

    kfree(ptr1);
    kfree(ptr2);
    kfree(ptr3);

    return true;
}

/* String operations test */
static bool test_string_operations(void)
{
    char buffer[256];
    const char *test_str = "Hello, M4KK1!";

    strcpy(buffer, test_str);
    if (strcmp(buffer, test_str) != 0) {
        return false;
    }

    if (strlen(buffer) != strlen(test_str)) {
        return false;
    }

    strcat(buffer, " Test");
    if (strcmp(buffer, "Hello, M4KK1! Test") != 0) {
        return false;
    }

    return true;
}

/* Process creation test */
static bool test_process_creation(void)
{
    mkrn_process_t *process = mkrn_process_create(
        "test_process", M4K_PRIO_NORMAL);
    if (!process) {
        return false;
    }

    if (process->pid == 0 ||
        strcmp(process->name, "test_process") != 0) {
        return false;
    }

    process_destroy(process);

    return true;
}

/* Math operations test */
static bool test_math_operations(void)
{
    uint32_t a = 100, b = 200, c = 0;

    c = a + b;
    if (c != 300) return false;

    c = b - a;
    if (c != 100) return false;

    c = a * b;
    if (c != 20000) return false;

    c = b / a;
    if (c != 2) return false;

    return true;
}

/* Initialize test framework */
void musr_test_framework_init(void)
{
    console_write("Initializing M4KK1 Test Framework...\n");

    musr_test_add_case(
        "Memory Allocation Test", test_memory_allocation);
    musr_test_add_case(
        "String Operations Test", test_string_operations);
    musr_test_add_case(
        "Process Creation Test", test_process_creation);
    musr_test_add_case(
        "Math Operations Test", test_math_operations);

    console_write("Test framework initialized\n");
}

/* Run test framework */
void musr_test_framework_run(void)
{
    musr_test_framework_init();
    musr_test_run_all();
    musr_test_print_results();
}
