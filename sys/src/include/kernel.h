/*
 * M4KK1 4P1 - kernel.h
 * Description: Core kernel data structures and function declarations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "multiboot.h"

#define M4K_MAGIC 0x4D344B4B

#define M4K_VERSION_MAJOR 0
#define M4K_VERSION_MINOR 1
#define M4K_VERSION_PATCH 0
#define M4K_VERSION_TYPE "devel"

#define M4K_MAX_PROCESSES 256
#define M4K_STACK_SIZE 32768
#define M4K_PAGE_SIZE 4096
#define M4K_HEAP_SIZE (1024 * 1024)

typedef struct {
    u32 magic;
    u32 version;
    char build_date[32];
    char build_time[32];
    u32 uptime_seconds;
    u32 process_count;
    u32 memory_total;
    u32 memory_free;
    u32 memory_used;
} mkrn_info_t;

/**
 * mkrn_main - Kernel entry point
 * @mb_info: Multiboot information structure
 * @magic: Bootloader magic number
 *
 * Return: void
 */
void mkrn_main(multiboot_info_t *mb_info, u32 magic);

/**
 * mkrn_panic - Trigger kernel panic
 * @message: Panic message
 *
 * Return: void
 */
void mkrn_panic(const char *message);

/**
 * mkrn_assert_failed - Handle assertion failure
 * @file: Source file name
 * @line: Line number
 * @expression: Failing expression
 *
 * Return: void
 */
void mkrn_assert_failed(const char *file, int line, const char *expression);

/**
 * mkrn_debug_dump - Dump kernel debug information
 *
 * Return: void
 */
void mkrn_debug_dump(void);

/**
 * mkrn_get_info - Get kernel information
 *
 * Return: Pointer to kernel info structure
 */
mkrn_info_t *mkrn_get_info(void);

/**
 * mkrn_sleep - Sleep for specified milliseconds
 * @milliseconds: Sleep duration in milliseconds
 *
 * Return: void
 */
void mkrn_sleep(u32 milliseconds);

/**
 * mkrn_busy_wait - Busy wait loop
 * @count: Iteration count
 *
 * Return: void
 */
void mkrn_busy_wait(u32 count);

/**
 * mkrn_alloc - Allocate kernel memory
 * @size: Allocation size in bytes
 *
 * Return: Pointer to allocated memory, NULL on failure
 */
void *mkrn_alloc(size_t size);

/**
 * mkrn_free - Free kernel memory
 * @ptr: Pointer to memory to free
 *
 * Return: void
 */
void mkrn_free(void *ptr);

/**
 * mkrn_strcpy - Copy string
 * @dest: Destination buffer
 * @src: Source string
 *
 * Return: void
 */
char *mkrn_strcpy(char *dest, const char *src);

/**
 * mkrn_strlen - Get string length
 * @str: Null-terminated string
 *
 * Return: Length of the string
 */
size_t mkrn_strlen(const char *str);

/**
 * mkrn_strcmp - Compare two strings
 * @str1: First string
 * @str2: Second string
 *
 * Return: 0 if equal, negative if str1 < str2, positive if str1 > str2
 */
int mkrn_strcmp(const char *str1, const char *str2);

/**
 * mkrn_memcpy - Copy memory block
 * @dest: Destination pointer
 * @src: Source pointer
 * @n: Number of bytes to copy
 *
 * Return: void
 */
void *mkrn_memcpy(void *dest, const void *src, size_t n);

/**
 * mkrn_memset - Set memory block to a value
 * @dest: Destination pointer
 * @value: Value to set
 * @n: Number of bytes to set
 *
 * Return: void
 */
void *mkrn_memset(void *dest, int value, size_t n);

#define M4K_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            mkrn_assert_failed(__FILE__, __LINE__, #expr); \
        } \
    } while (0)

#define M4K_VERIFY_MAGIC(magic) \
    M4K_ASSERT((magic) == M4K_MAGIC)

#define M4K_VERSION_STRING \
    "Y4KU-" \
    M4K_STRINGIFY(M4K_VERSION_MAJOR) "." \
    M4K_STRINGIFY(M4K_VERSION_MINOR) "." \
    M4K_STRINGIFY(M4K_VERSION_PATCH) "-" \
    M4K_VERSION_TYPE

#define M4K_STRINGIFY(x) _M4K_STRINGIFY(x)
#define _M4K_STRINGIFY(x) #x

#define M4K_BUILD_TIME __TIME__
#define M4K_BUILD_DATE __DATE__

#define M4K_CLI() __asm__ volatile ("cli")
#define M4K_STI() __asm__ volatile ("sti")
#define M4K_HLT() __asm__ volatile ("hlt")
#define M4K_PAUSE() __asm__ volatile ("pause")
#define M4K_MEMORY_BARRIER() __asm__ volatile ("" ::: "memory")
#define M4K_READ_BARRIER() __asm__ volatile ("" ::: "memory")
#define M4K_WRITE_BARRIER() __asm__ volatile ("" ::: "memory")

#define M4K_LOG_DEBUG(msg) \
    do { \
        mkrn_console_write("[DEBUG] "); \
        mkrn_console_write(msg); \
        mkrn_console_write("\n"); \
    } while (0)

#define M4K_LOG_INFO(msg) \
    do { \
        mkrn_console_write("[INFO] "); \
        mkrn_console_write(msg); \
        mkrn_console_write("\n"); \
    } while (0)

#define M4K_LOG_WARN(msg) \
    do { \
        mkrn_console_write("[WARN] "); \
        mkrn_console_write(msg); \
        mkrn_console_write("\n"); \
    } while (0)

#define M4K_LOG_ERROR(msg) \
    do { \
        mkrn_console_write("[ERROR] "); \
        mkrn_console_write(msg); \
        mkrn_console_write("\n"); \
    } while (0)

#define M4K_PANIC(msg) \
    do { \
        mkrn_console_panic(msg); \
        while (1) { \
            __asm__ volatile ("cli; hlt"); \
        } \
    } while (0)

#define M4K_MEMORY_PANIC(msg) \
    do { \
        mkrn_console_memory_error(msg); \
        while (1) { \
            __asm__ volatile ("cli; hlt"); \
        } \
    } while (0)

#define M4K_SYSTEM_PANIC(msg) \
    do { \
        mkrn_console_system_error(msg); \
        while (1) { \
            __asm__ volatile ("cli; hlt"); \
        } \
    } while (0)
