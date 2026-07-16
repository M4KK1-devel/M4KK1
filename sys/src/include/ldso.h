/*
 * M4KK1 4P1 - ldso.h
 * Description: Dynamic linker and library loading interface.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#define M4K_LL_MAGIC 0x4D344C4C

#define M4K_LL_SEGMENT_CODE   1
#define M4K_LL_SEGMENT_DATA   2
#define M4K_LL_SEGMENT_BSS    3
#define M4K_LL_SEGMENT_RODATA 4

#define M4K_LL_SYMBOL_LOCAL   0
#define M4K_LL_SYMBOL_GLOBAL  1
#define M4K_LL_SYMBOL_WEAK    2

#define M4K_LL_SYMBOL_FUNCTION 0
#define M4K_LL_SYMBOL_OBJECT   1

#define M4K_LL_RELOCATION_32   1
#define M4K_LL_RELOCATION_PC32 2
#define M4K_LL_RELOCATION_GOT32 3
#define M4K_LL_RELOCATION_PLT32 4

#define M4K_LL_STATUS_UNLOADED  0
#define M4K_LL_STATUS_LOADING   1
#define M4K_LL_STATUS_LOADED    2
#define M4K_LL_STATUS_RELOCATED 3
#define M4K_LL_STATUS_ERROR     4

typedef struct {
    u32 magic;
    u32 version;
    u32 flags;
    u32 entry_point;
    u32 phdr_offset;
    u32 phdr_count;
    u32 shdr_offset;
    u32 shdr_count;
    u32 strtab_offset;
    u32 strtab_size;
    u32 symtab_offset;
    u32 symtab_count;
    u32 rel_offset;
    u32 rel_count;
    u32 dep_offset;
    u32 dep_count;
    u32 checksum;
} mkrn_ll_header_t;

typedef struct {
    u32 type;
    u32 offset;
    u32 vaddr;
    u32 paddr;
    u32 file_size;
    u32 mem_size;
    u32 flags;
    u32 align;
} mkrn_ll_phdr_t;

typedef struct {
    u32 name_offset;
    u32 type;
    u32 flags;
    u32 addr;
    u32 offset;
    u32 size;
    u32 link;
    u32 info;
    u32 align;
    u32 entry_size;
} mkrn_ll_shdr_t;

typedef struct {
    u32 name_offset;
    u32 value;
    u32 size;
    u8 type;
    u8 binding;
    u8 visibility;
    u8 section;
} mkrn_ll_sym_t;

typedef struct {
    u32 offset;
    u32 info;
    u32 sym_index;
    s32 addend;
} mkrn_ll_rel_t;

typedef struct {
    u32 name_offset;
    u32 version;
    u32 flags;
} mkrn_ll_dep_t;

typedef struct mkrn_ll_library {
    char *name;
    void *base_addr;
    u32 status;
    mkrn_ll_header_t *header;
    mkrn_ll_sym_t *symtab;
    char *strtab;
    mkrn_ll_rel_t *reltab;
    mkrn_ll_dep_t *deptab;
    u32 ref_count;
    struct mkrn_ll_library *next;
    struct mkrn_ll_library *deps;
} mkrn_ll_library_t;

typedef struct mkrn_ll_symbol {
    char *name;
    void *address;
    u32 size;
    u32 type;
    u32 binding;
    mkrn_ll_library_t *library;
    struct mkrn_ll_symbol *next;
} mkrn_ll_symbol_t;

typedef struct {
    mkrn_ll_library_t *loaded_libs;
    mkrn_ll_symbol_t *global_symbols;
    u32 base_address;
    u32 flags;
} mkrn_ll_context_t;

/**
 * mkrn_ll_load_library - Load a dynamic library
 * @filename: Library file name
 * @lib: Output pointer for loaded library
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_ll_load_library(const char *filename, mkrn_ll_library_t **lib);

/**
 * mkrn_ll_unload_library - Unload a dynamic library
 * @lib: Library to unload
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_ll_unload_library(mkrn_ll_library_t *lib);

/**
 * mkrn_ll_find_symbol - Find a symbol by name
 * @name: Symbol name
 *
 * Return: Symbol address, NULL if not found
 */
void *mkrn_ll_find_symbol(const char *name);

/**
 * mkrn_ll_add_symbol - Add a symbol to the global table
 * @name: Symbol name
 * @address: Symbol address
 * @type: Symbol type
 * @binding: Symbol binding
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_ll_add_symbol(const char *name, void *address, u32 type, u32 binding);

/**
 * mkrn_ll_perform_relocations - Perform relocations for a library
 * @lib: Library to relocate
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_ll_perform_relocations(mkrn_ll_library_t *lib);

/**
 * mkrn_ll_resolve_dependencies - Resolve library dependencies
 * @lib: Library to resolve
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_ll_resolve_dependencies(mkrn_ll_library_t *lib);

/**
 * mkrn_ll_allocate_memory - Allocate memory for library loading
 * @size: Allocation size
 * @flags: Allocation flags
 *
 * Return: Pointer to allocated memory, NULL on failure
 */
void *mkrn_ll_allocate_memory(size_t size, u32 flags);

/**
 * mkrn_ll_free_memory - Free library-allocated memory
 * @ptr: Pointer to free
 *
 * Return: void
 */
void mkrn_ll_free_memory(void *ptr);

/**
 * mkrn_ll_init - Initialize the dynamic linker
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_ll_init(void);

/**
 * mkrn_ll_cleanup - Clean up the dynamic linker
 *
 * Return: void
 */
void mkrn_ll_cleanup(void);

/**
 * mkrn_ll_hash_string - Hash a string
 * @str: String to hash
 *
 * Return: Hash value
 */
u32 mkrn_ll_hash_string(const char *str);

/**
 * mkrn_ll_strcmp - Compare two strings
 * @s1: First string
 * @s2: Second string
 *
 * Return: 0 if equal, difference otherwise
 */
int mkrn_ll_strcmp(const char *s1, const char *s2);

/**
 * mkrn_ll_memcpy - Copy memory block
 * @dest: Destination pointer
 * @src: Source pointer
 * @n: Number of bytes
 *
 * Return: Pointer to destination
 */
void *mkrn_ll_memcpy(void *dest, const void *src, size_t n);

/**
 * mkrn_ll_memset - Set memory block
 * @s: Memory pointer
 * @c: Value to set
 * @n: Number of bytes
 *
 * Return: Pointer to memory
 */
void *mkrn_ll_memset(void *s, int c, size_t n);

extern int mkrn_ll_errno;
extern char mkrn_ll_error_msg[256];

#define M4K_LL_ERROR_NONE         0
#define M4K_LL_ERROR_FILE_NOT_FOUND 1
#define M4K_LL_ERROR_INVALID_FORMAT 2
#define M4K_LL_ERROR_LOAD_FAILED    3
#define M4K_LL_ERROR_SYMBOL_NOT_FOUND 4
#define M4K_LL_ERROR_RELOCATION_FAILED 5
#define M4K_LL_ERROR_DEPENDENCY_FAILED 6
#define M4K_LL_ERROR_MEMORY_FAILED  7
