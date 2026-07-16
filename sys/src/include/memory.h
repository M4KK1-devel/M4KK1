/*
 * M4KK1 4P1 - memory.h
 * Description: Memory management function declarations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "multiboot.h"

#define M4K_MEM_TYPE_FREE        1
#define M4K_MEM_TYPE_RESERVED    2
#define M4K_MEM_TYPE_ACPI        3
#define M4K_MEM_TYPE_NVS         4
#define M4K_MEM_TYPE_BAD         5

typedef struct mkrn_mem_region {
    u32 start;
    u32 size;
    u32 type;
    struct mkrn_mem_region *next;
} mkrn_mem_region_t;

typedef struct mkrn_mem_block {
    u32 start;
    u32 size;
    u8 used;
    struct mkrn_mem_block *next;
} mkrn_mem_block_t;

#define M4K_PAGE_SIZE 4096
#define M4K_PAGE_MASK (~(M4K_PAGE_SIZE - 1))

#define M4K_PAGE_PRESENT    0x001
#define M4K_PAGE_READWRITE  0x002
#define M4K_PAGE_USER       0x004
#define M4K_PAGE_ACCESSED   0x020
#define M4K_PAGE_DIRTY      0x040

#define M4K_KERNEL_BASE     0xC0000000
#define M4K_KERNEL_HEAP     0xC0400000
#define M4K_KERNEL_STACK    0xC07FE000

/**
 * mkrn_memory_init - Initialize memory management
 * @mb_info: Multiboot information structure
 *
 * Return: void
 */
void mkrn_memory_init(multiboot_info_t *mb_info);

/**
 * mkrn_memory_get_total - Get total memory size
 *
 * Return: Total memory in bytes
 */
u32 mkrn_memory_get_total(void);

/**
 * mkrn_memory_get_free - Get free memory size
 *
 * Return: Free memory in bytes
 */
u32 mkrn_memory_get_free(void);

/**
 * mkrn_memory_get_used - Get used memory size
 *
 * Return: Used memory in bytes
 */
u32 mkrn_memory_get_used(void);

/**
 * mkrn_memory_alloc - Allocate memory
 * @size: Allocation size
 *
 * Return: Pointer to allocated memory, NULL on failure
 */
void *mkrn_memory_alloc(size_t size);

/**
 * mkrn_memory_free - Free allocated memory
 * @ptr: Pointer to free
 *
 * Return: void
 */
void mkrn_memory_free(void *ptr);

/**
 * mkrn_memory_alloc_page - Allocate page-aligned memory
 * @pages: Number of pages
 *
 * Return: Pointer to allocated memory, NULL on failure
 */
void *mkrn_memory_alloc_page(size_t pages);

/**
 * mkrn_memory_free_page - Free page-aligned memory
 * @ptr: Pointer to free
 * @pages: Number of pages
 *
 * Return: void
 */
void mkrn_memory_free_page(void *ptr, size_t pages);

/**
 * mkrn_alloc - Allocate kernel memory
 * @size: Allocation size
 *
 * Return: Pointer to allocated memory, NULL on failure
 */
void *mkrn_alloc(size_t size);

/**
 * mkrn_free - Free kernel memory
 * @ptr: Pointer to free
 *
 * Return: void
 */
void mkrn_free(void *ptr);

/**
 * mkrn_memcpy - Copy memory block
 * @dest: Destination
 * @src: Source
 * @n: Number of bytes
 *
 * Return: Pointer to destination
 */
void *mkrn_memcpy(void *dest, const void *src, size_t n);

/**
 * mkrn_memset - Set memory block
 * @s: Memory pointer
 * @c: Value to set
 * @n: Number of bytes
 *
 * Return: Pointer to memory
 */
void *mkrn_memset(void *s, int c, size_t n);

/**
 * mkrn_memcmp - Compare memory blocks
 * @s1: First block
 * @s2: Second block
 * @n: Number of bytes
 *
 * Return: 0 if equal, difference otherwise
 */
int mkrn_memcmp(const void *s1, const void *s2, size_t n);

/**
 * mkrn_memmove - Move memory block
 * @dest: Destination
 * @src: Source
 * @n: Number of bytes
 *
 * Return: Pointer to destination
 */
void *mkrn_memmove(void *dest, const void *src, size_t n);

/**
 * mkrn_memchr - Find character in memory
 * @s: Memory pointer
 * @c: Character to find
 * @n: Number of bytes
 *
 * Return: Pointer to character, NULL if not found
 */
void *mkrn_memchr(const void *s, int c, size_t n);

/**
 * mkrn_strlen - Get string length
 * @s: Null-terminated string
 *
 * Return: Length of string
 */
size_t mkrn_strlen(const char *s);

/**
 * mkrn_strcpy - Copy string
 * @dest: Destination buffer
 * @src: Source string
 *
 * Return: Pointer to destination
 */
char *mkrn_strcpy(char *dest, const char *src);

/**
 * mkrn_strcat - Concatenate strings
 * @dest: Destination buffer
 * @src: Source string
 *
 * Return: Pointer to destination
 */
char *mkrn_strcat(char *dest, const char *src);

/**
 * mkrn_strcmp - Compare strings
 * @s1: First string
 * @s2: Second string
 *
 * Return: 0 if equal, difference otherwise
 */
int mkrn_strcmp(const char *s1, const char *s2);

/**
 * mkrn_strncpy - Copy string with length limit
 * @dest: Destination buffer
 * @src: Source string
 * @n: Maximum length
 *
 * Return: Pointer to destination
 */
char *mkrn_strncpy(char *dest, const char *src, size_t n);

/**
 * mkrn_strncat - Concatenate strings with length limit
 * @dest: Destination buffer
 * @src: Source string
 * @n: Maximum length
 *
 * Return: Pointer to destination
 */
char *mkrn_strncat(char *dest, const char *src, size_t n);

/**
 * mkrn_strncmp - Compare strings with length limit
 * @s1: First string
 * @s2: Second string
 * @n: Maximum length
 *
 * Return: 0 if equal, difference otherwise
 */
int mkrn_strncmp(const char *s1, const char *s2, size_t n);

/**
 * mkrn_strchr - Find character in string
 * @s: String to search
 * @c: Character to find
 *
 * Return: Pointer to character, NULL if not found
 */
char *mkrn_strchr(const char *s, int c);

/**
 * mkrn_strstr - Find substring in string
 * @haystack: String to search
 * @needle: Substring to find
 *
 * Return: Pointer to substring, NULL if not found
 */
char *mkrn_strstr(const char *haystack, const char *needle);
