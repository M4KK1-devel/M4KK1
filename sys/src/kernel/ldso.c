/*
 * M4KK1 4P1 - ldso.c
 * Description: Dynamic linker/loader for M4KK1 —
 *              .m4ll format parsing, symbol resolution,
 *              relocation, dependency management.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "ldso.h"
#include "process.h"
#include "memory.h"
#include "kernel.h"
#include "console.h"
#include <string.h>
#include <stdint.h>

/* 全局错误状态 */
int mkrn_ll_errno = M4K_LL_ERROR_NONE;
char mkrn_ll_error_msg[256] = {0};

/* 全局上下文 */
static mkrn_ll_context_t ldso_context;

/* 字符串哈希函数 */
uint32_t mkrn_ll_hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

/* 字符串比较函数 */
int mkrn_ll_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* 内存复制函数 */
void *mkrn_ll_memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;

    while (n--) {
        *d++ = *s++;
    }

    return dest;
}

/* 内存设置函数 */
void *mkrn_ll_memset(void *s, int c, size_t n) {
    unsigned char *p = s;

    while (n--) {
        *p++ = (unsigned char)c;
    }

    return s;
}

/* 设置错误信息 */
static void set_error(int error, const char *msg) {
    mkrn_ll_errno = error;
    mkrn_strncpy(mkrn_ll_error_msg, msg, sizeof(mkrn_ll_error_msg) - 1);
    mkrn_ll_error_msg[sizeof(mkrn_ll_error_msg) - 1] = '\0';
    M4K_LOG_ERROR("LDSO Error");
}

/* 验证文件头 */
static int validate_header(mkrn_ll_header_t *header) {
    if (header->magic != M4K_LL_MAGIC) {
        set_error(M4K_LL_ERROR_INVALID_FORMAT, "Invalid magic number");
        return -1;
    }

    if (header->version != 1) {
        set_error(M4K_LL_ERROR_INVALID_FORMAT, "Unsupported version");
        return -1;
    }

    /* 验证校验和 */
    uint32_t checksum = header->checksum;
    header->checksum = 0;
    uint32_t computed = 0; /* 简化的校验和计算 */

    for (int i = 0; i < sizeof(mkrn_ll_header_t) / 4; i++) {
        computed += ((uint32_t*)header)[i];
    }

    header->checksum = checksum;

    if (computed != checksum) {
        set_error(M4K_LL_ERROR_INVALID_FORMAT, "Header checksum mismatch");
        return -1;
    }

    return 0;
}

/* 读取文件内容到内存 */
static void *read_file_to_memory(const char *filename, size_t *size) {
    /* 这里需要实现文件读取逻辑 */
    /* 暂时返回NULL，表示功能未完全实现 */
    set_error(M4K_LL_ERROR_FILE_NOT_FOUND, "File I/O not implemented");
    return NULL;
}

/* 分配库结构 */
static mkrn_ll_library_t *alloc_library(void) {
    mkrn_ll_library_t *lib = (mkrn_ll_library_t *)mkrn_alloc(sizeof(mkrn_ll_library_t));
    if (!lib) {
        set_error(M4K_LL_ERROR_MEMORY_FAILED, "Failed to allocate library structure");
        return NULL;
    }

    mkrn_memset(lib, 0, sizeof(mkrn_ll_library_t));
    lib->status = M4K_LL_STATUS_UNLOADED;
    lib->ref_count = 1;

    return lib;
}

/* 释放库结构 */
static void free_library(mkrn_ll_library_t *lib) {
    if (!lib) return;

    if (lib->name) mkrn_free(lib->name);
    if (lib->header) mkrn_free(lib->header);
    if (lib->symtab) mkrn_free(lib->symtab);
    if (lib->strtab) mkrn_free(lib->strtab);
    if (lib->reltab) mkrn_free(lib->reltab);
    if (lib->deptab) mkrn_free(lib->deptab);

    mkrn_free(lib);
}

/* 解析文件头 */
static int parse_header(mkrn_ll_library_t *lib, void *file_data) {
    mkrn_ll_header_t *header = (mkrn_ll_header_t *)file_data;

    if (validate_header(header) < 0) {
        return -1;
    }

    /* 复制文件头 */
    lib->header = (mkrn_ll_header_t *)mkrn_alloc(sizeof(mkrn_ll_header_t));
    if (!lib->header) {
        set_error(M4K_LL_ERROR_MEMORY_FAILED, "Failed to allocate header");
        return -1;
    }

    mkrn_memcpy(lib->header, header, sizeof(mkrn_ll_header_t));

    return 0;
}

/* 解析符号表 */
static int parse_symbol_table(mkrn_ll_library_t *lib, void *file_data) {
    mkrn_ll_header_t *header = lib->header;

    if (header->symtab_count == 0) {
        return 0; /* 没有符号表 */
    }

    /* 分配符号表 */
    lib->symtab = (mkrn_ll_sym_t *)mkrn_alloc(header->symtab_count * sizeof(mkrn_ll_sym_t));
    if (!lib->symtab) {
        set_error(M4K_LL_ERROR_MEMORY_FAILED, "Failed to allocate symbol table");
        return -1;
    }

    /* 复制符号表 */
    void *symtab_src = (uint8_t *)file_data + header->symtab_offset;
    mkrn_memcpy(lib->symtab, symtab_src, header->symtab_count * sizeof(mkrn_ll_sym_t));

    return 0;
}

/* 解析字符串表 */
static int parse_string_table(mkrn_ll_library_t *lib, void *file_data) {
    mkrn_ll_header_t *header = lib->header;

    if (header->strtab_size == 0) {
        return 0; /* 没有字符串表 */
    }

    /* 分配字符串表 */
    lib->strtab = (char *)mkrn_alloc(header->strtab_size);
    if (!lib->strtab) {
        set_error(M4K_LL_ERROR_MEMORY_FAILED, "Failed to allocate string table");
        return -1;
    }

    /* 复制字符串表 */
    void *strtab_src = (uint8_t *)file_data + header->strtab_offset;
    mkrn_memcpy(lib->strtab, strtab_src, header->strtab_size);

    return 0;
}

/* 解析重定位表 */
static int parse_relocation_table(mkrn_ll_library_t *lib, void *file_data) {
    mkrn_ll_header_t *header = lib->header;

    if (header->rel_count == 0) {
        return 0; /* 没有重定位表 */
    }

    /* 分配重定位表 */
    lib->reltab = (mkrn_ll_rel_t *)mkrn_alloc(header->rel_count * sizeof(mkrn_ll_rel_t));
    if (!lib->reltab) {
        set_error(M4K_LL_ERROR_MEMORY_FAILED, "Failed to allocate relocation table");
        return -1;
    }

    /* 复制重定位表 */
    void *reltab_src = (uint8_t *)file_data + header->rel_offset;
    mkrn_memcpy(lib->reltab, reltab_src, header->rel_count * sizeof(mkrn_ll_rel_t));

    return 0;
}

/* 解析依赖表 */
static int parse_dependency_table(mkrn_ll_library_t *lib, void *file_data) {
    mkrn_ll_header_t *header = lib->header;

    if (header->dep_count == 0) {
        return 0; /* 没有依赖 */
    }

    /* 分配依赖表 */
    lib->deptab = (mkrn_ll_dep_t *)mkrn_alloc(header->dep_count * sizeof(mkrn_ll_dep_t));
    if (!lib->deptab) {
        set_error(M4K_LL_ERROR_MEMORY_FAILED, "Failed to allocate dependency table");
        return -1;
    }

    /* 复制依赖表 */
    void *deptab_src = (uint8_t *)file_data + header->dep_offset;
    mkrn_memcpy(lib->deptab, deptab_src, header->dep_count * sizeof(mkrn_ll_dep_t));

    return 0;
}

/* 加载库到内存 */
static int load_library_data(mkrn_ll_library_t *lib, void *file_data) {
    mkrn_ll_header_t *header = lib->header;

    /* 分配基地址 */
    lib->base_addr = (void *)ldso_context.base_address;
    ldso_context.base_address += 0x100000; /* 增加4MB */

    /* 解析各个表 */
    if (parse_symbol_table(lib, file_data) < 0) return -1;
    if (parse_string_table(lib, file_data) < 0) return -1;
    if (parse_relocation_table(lib, file_data) < 0) return -1;
    if (parse_dependency_table(lib, file_data) < 0) return -1;

    return 0;
}

/* 添加全局符号 */
static int add_global_symbol(const char *name, void *address, uint32_t type, uint32_t binding) {
    mkrn_ll_symbol_t *symbol = (mkrn_ll_symbol_t *)mkrn_alloc(sizeof(mkrn_ll_symbol_t));
    if (!symbol) {
        set_error(M4K_LL_ERROR_MEMORY_FAILED, "Failed to allocate symbol structure");
        return -1;
    }

    symbol->name = mkrn_strdup(name); /* 需要实现mkrn_strdup */
    if (!symbol->name) {
        mkrn_free(symbol);
        set_error(M4K_LL_ERROR_MEMORY_FAILED, "Failed to duplicate symbol name");
        return -1;
    }

    symbol->address = address;
    symbol->size = 0; /* 暂时设为0 */
    symbol->type = type;
    symbol->binding = binding;
    symbol->library = NULL; /* 暂时设为NULL */
    symbol->next = ldso_context.global_symbols;

    ldso_context.global_symbols = symbol;

    return 0;
}

/* 查找全局符号 */
void *mkrn_ll_find_symbol(const char *name) {
    mkrn_ll_symbol_t *symbol = ldso_context.global_symbols;

    while (symbol) {
        if (mkrn_ll_strcmp(symbol->name, name) == 0) {
            return symbol->address;
        }
        symbol = symbol->next;
    }

    return NULL;
}

/* 添加符号到全局符号表 */
int mkrn_ll_add_symbol(const char *name, void *address, uint32_t type, uint32_t binding) {
    return add_global_symbol(name, address, type, binding);
}

/* 执行重定位 */
static int perform_relocations(mkrn_ll_library_t *lib) {
    mkrn_ll_header_t *header = lib->header;
    uint32_t i;

    for (i = 0; i < header->rel_count; i++) {
        mkrn_ll_rel_t *rel = &lib->reltab[i];
        mkrn_ll_sym_t *sym = &lib->symtab[rel->sym_index];
        char *sym_name = &lib->strtab[sym->name_offset];

        /* 查找符号地址 */
        void *sym_addr = mkrn_ll_find_symbol(sym_name);
        if (!sym_addr && sym->binding != M4K_LL_SYMBOL_WEAK) {
            set_error(M4K_LL_ERROR_SYMBOL_NOT_FOUND, "Symbol not found");
            return -1;
        }

        /* 计算重定位地址 */
        uint32_t *rel_addr = (uint32_t *)((uint8_t *)lib->base_addr + rel->offset);

        /* 执行重定位 */
        switch (rel->info & 0xFF) { /* 重定位类型 */
            case M4K_LL_RELOCATION_32:
                *rel_addr = (uint32_t)sym_addr + rel->addend;
                break;

            case M4K_LL_RELOCATION_PC32:
                *rel_addr = (uint32_t)sym_addr + rel->addend - (uint32_t)rel_addr;
                break;

            default:
                set_error(M4K_LL_ERROR_RELOCATION_FAILED, "Unknown relocation type");
                return -1;
        }
    }

    return 0;
}

/* 加载依赖库 */
static int load_dependencies(mkrn_ll_library_t *lib) {
    mkrn_ll_header_t *header = lib->header;
    uint32_t i;

    for (i = 0; i < header->dep_count; i++) {
        mkrn_ll_dep_t *dep = &lib->deptab[i];
        char *dep_name = &lib->strtab[dep->name_offset];

        /* 递归加载依赖库 */
        mkrn_ll_library_t *dep_lib;
        if (mkrn_ll_load_library(dep_name, &dep_lib) < 0) {
            set_error(M4K_LL_ERROR_DEPENDENCY_FAILED, "Failed to load dependency");
            return -1;
        }

        /* 添加到依赖链表 */
        dep_lib->next = lib->deps;
        lib->deps = dep_lib;
    }

    return 0;
}

/* 加载动态库 */
int mkrn_ll_load_library(const char *filename, mkrn_ll_library_t **lib) {
    void *file_data;
    size_t file_size;
    mkrn_ll_library_t *library;

    M4K_LOG_INFO("Loading dynamic library");

    /* 读取文件 */
    file_data = read_file_to_memory(filename, &file_size);
    if (!file_data) {
        return -1;
    }

    /* 分配库结构 */
    library = alloc_library();
    if (!library) {
        mkrn_free(file_data);
        return -1;
    }

    /* 设置库名称 */
    library->name = mkrn_strdup(filename); /* 需要实现mkrn_strdup */
    if (!library->name) {
        free_library(library);
        mkrn_free(file_data);
        set_error(M4K_LL_ERROR_MEMORY_FAILED, "Failed to duplicate library name");
        return -1;
    }

    /* 解析文件头 */
    if (parse_header(library, file_data) < 0) {
        free_library(library);
        mkrn_free(file_data);
        return -1;
    }

    /* 加载库数据 */
    if (load_library_data(library, file_data) < 0) {
        free_library(library);
        mkrn_free(file_data);
        return -1;
    }

    /* 加载依赖 */
    if (load_dependencies(library) < 0) {
        free_library(library);
        mkrn_free(file_data);
        return -1;
    }

    /* 执行重定位 */
    if (perform_relocations(library) < 0) {
        free_library(library);
        mkrn_free(file_data);
        return -1;
    }

    /* 更新状态 */
    library->status = M4K_LL_STATUS_LOADED;

    /* 添加到已加载库链表 */
    library->next = ldso_context.loaded_libs;
    ldso_context.loaded_libs = library;

    *lib = library;

    mkrn_free(file_data);

    M4K_LOG_INFO("Library loaded successfully");

    return 0;
}

/* 卸载动态库 */
int mkrn_ll_unload_library(mkrn_ll_library_t *lib) {
    mkrn_ll_library_t *prev, *curr;

    if (!lib) return 0;

    /* 减少引用计数 */
    lib->ref_count--;
    if (lib->ref_count > 0) {
        return 0; /* 还有其他引用 */
    }

    /* 从已加载库链表中移除 */
    prev = NULL;
    curr = ldso_context.loaded_libs;

    while (curr) {
        if (curr == lib) {
            if (prev) {
                prev->next = curr->next;
            } else {
                ldso_context.loaded_libs = curr->next;
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    /* 释放库资源 */
    free_library(lib);

    M4K_LOG_INFO("Library unloaded");

    return 0;
}

/* 初始化动态链接器 */
int mkrn_ll_init(void) {
    M4K_LOG_INFO("Initializing dynamic linker...");

    /* 初始化全局上下文 */
    mkrn_memset(&ldso_context, 0, sizeof(mkrn_ll_context_t));
    ldso_context.base_address = 0xD0000000; /* 库加载基地址 */

    /* 清空错误状态 */
    mkrn_ll_errno = M4K_LL_ERROR_NONE;
    mkrn_ll_error_msg[0] = '\0';

    M4K_LOG_INFO("Dynamic linker initialized");

    return 0;
}

/* 清理动态链接器 */
void mkrn_ll_cleanup(void) {
    mkrn_ll_library_t *lib, *next;

    M4K_LOG_INFO("Cleaning up dynamic linker...");

    /* 卸载所有库 */
    lib = ldso_context.loaded_libs;
    while (lib) {
        next = lib->next;
        mkrn_ll_unload_library(lib);
        lib = next;
    }

    /* 清空全局符号表 */
    mkrn_ll_symbol_t *symbol, *next_symbol;

    symbol = ldso_context.global_symbols;
    while (symbol) {
        next_symbol = symbol->next;
        if (symbol->name) mkrn_free(symbol->name);
        mkrn_free(symbol);
        symbol = next_symbol;
    }

    M4K_LOG_INFO("Dynamic linker cleanup completed");
}

/* 内存分配函数 */
void *mkrn_ll_allocate_memory(size_t size, uint32_t flags) {
    return mkrn_alloc(size);
}

/* 内存释放函数 */
void mkrn_ll_free_memory(void *ptr) {
    mkrn_free(ptr);
}
