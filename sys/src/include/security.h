/*
 * M4KK1 4P1 - security.h
 * Description: Security framework header definitions.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define M4K_SEC_MODE_DISABLED  0
#define M4K_SEC_MODE_PERMISSIVE 1
#define M4K_SEC_MODE_ENFORCING  2

#define M4K_SEC_USER_SYSTEM    "system_u"
#define M4K_SEC_USER_ROOT      "root_u"
#define M4K_SEC_USER_USER      "user_u"

#define M4K_SEC_ROLE_SYSTEM    "system_r"
#define M4K_SEC_ROLE_OBJECT    "object_r"
#define M4K_SEC_ROLE_USER      "user_r"

#define M4K_SEC_TYPE_KERNEL    "kernel_t"
#define M4K_SEC_TYPE_INIT      "init_t"
#define M4K_SEC_TYPE_SHELL     "shell_t"
#define M4K_SEC_TYPE_FILE      "file_t"
#define M4K_SEC_TYPE_UNCONFINED "unconfined_t"

#define M4K_SEC_READ          (1 << 0)
#define M4K_SEC_WRITE         (1 << 1)
#define M4K_SEC_EXECUTE       (1 << 2)
#define M4K_SEC_CREATE        (1 << 3)
#define M4K_SEC_DELETE        (1 << 4)
#define M4K_SEC_RENAME        (1 << 5)
#define M4K_SEC_LINK          (1 << 6)
#define M4K_SEC_UNLINK        (1 << 7)
#define M4K_SEC_IOCTL         (1 << 8)
#define M4K_SEC_LOCK          (1 << 9)
#define M4K_SEC_SEARCH        (1 << 10)
#define M4K_SEC_ADD_NAME      (1 << 11)
#define M4K_SEC_REMOVE_NAME   (1 << 12)
#define M4K_SEC_REPARENT      (1 << 13)
#define M4K_SEC_GETATTR       (1 << 14)
#define M4K_SEC_SETATTR       (1 << 15)
#define M4K_SEC_LIST_DIR      (1 << 16)
#define M4K_SEC_MOUNT         (1 << 17)
#define M4K_SEC_UMOUNT        (1 << 18)
#define M4K_SEC_RELOAD        (1 << 19)
#define M4K_SEC_KILL          (1 << 20)
#define M4K_SEC_SIGNAL        (1 << 21)
#define M4K_SEC_MODULE_LOAD   (1 << 22)
#define M4K_SEC_MODULE_UNLOAD (1 << 23)

#define M4K_SEC_CLASS_FILE     1
#define M4K_SEC_CLASS_DIR      2
#define M4K_SEC_CLASS_LNK_FILE 3
#define M4K_SEC_CLASS_CHR_FILE 4
#define M4K_SEC_CLASS_BLK_FILE 5
#define M4K_SEC_CLASS_SOCK_FILE 6
#define M4K_SEC_CLASS_FIFO_FILE 7
#define M4K_SEC_CLASS_SOCKET   8
#define M4K_SEC_CLASS_TCP_SOCKET 9
#define M4K_SEC_CLASS_UDP_SOCKET 10
#define M4K_SEC_CLASS_PROCESS  11
#define M4K_SEC_CLASS_THREAD   12
#define M4K_SEC_CLASS_SYSTEM   13
#define M4K_SEC_CLASS_CAPABILITY 14
#define M4K_SEC_CLASS_MEMPROTECT 15

#define M4K_SEC_GRANTED        0
#define M4K_SEC_DENIED         1
#define M4K_SEC_UNKNOWN        2

typedef u32 mkrn_sec_id_t;

typedef struct {
    char user[64];
    char role[64];
    char type[64];
    char level[64];
} mkrn_sec_context_t;

typedef struct mkrn_sec_rule {
    char source_type[64];
    char target_type[64];
    char object_class[64];
    u32 permissions;
    struct mkrn_sec_rule *next;
} mkrn_sec_rule_t;

/**
 * mkrn_security_init - Initialize the security framework
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_security_init(void);

/**
 * mkrn_security_set_mode - Set security mode
 * @enforcing: True for enforcing mode, false for permissive
 *
 * Return: void
 */
void mkrn_security_set_mode(b enforcing);

/**
 * mkrn_security_get_mode - Get current security mode
 *
 * Return: True if enforcing, false if permissive
 */
b mkrn_security_get_mode(void);

/**
 * mkrn_security_cleanup - Clean up the security framework
 *
 * Return: void
 */
void mkrn_security_cleanup(void);

/**
 * mkrn_security_create_context - Create a security context
 * @user: User identifier
 * @role: Role identifier
 * @type: Type identifier
 * @level: Sensitivity level
 *
 * Return: Security ID, 0 on failure
 */
mkrn_sec_id_t mkrn_security_create_context(const char *user, const char *role,
                                           const char *type, const char *level);

/**
 * mkrn_security_destroy_context - Destroy a security context
 * @sid: Security ID
 *
 * Return: void
 */
void mkrn_security_destroy_context(mkrn_sec_id_t sid);

/**
 * mkrn_security_add_rule - Add a security policy rule
 * @source_type: Source type
 * @target_type: Target type
 * @object_class: Object class
 * @permissions: Permission bitmask
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_security_add_rule(const char *source_type, const char *target_type,
                           const char *object_class, u32 permissions);

/**
 * mkrn_security_load_default_policy - Load default security policy
 *
 * Return: void
 */
void mkrn_security_load_default_policy(void);

/**
 * mkrn_security_check_access - Check access permission
 * @sid: Source security ID
 * @tsid: Target security ID
 * @object_class: Object class name
 * @permission: Requested permission
 *
 * Return: True if access granted
 */
b mkrn_security_check_access(mkrn_sec_id_t sid, mkrn_sec_id_t tsid,
                             const char *object_class, u32 permission);

/**
 * mkrn_security_check_file_access - Check file access permission
 * @sid: Source security ID
 * @path: File path
 * @permission: Requested permission
 *
 * Return: True if access granted
 */
b mkrn_security_check_file_access(mkrn_sec_id_t sid, const char *path, u32 permission);

/**
 * mkrn_security_check_process_access - Check process access permission
 * @sid: Source security ID
 * @tsid: Target security ID
 * @permission: Requested permission
 *
 * Return: True if access granted
 */
b mkrn_security_check_process_access(mkrn_sec_id_t sid, mkrn_sec_id_t tsid, u32 permission);

/**
 * mkrn_security_get_process_context - Get current process security context
 *
 * Return: Security ID
 */
mkrn_sec_id_t mkrn_security_get_process_context(void);

/**
 * mkrn_security_set_file_context - Set file security context
 * @path: File path
 * @sid: Security ID
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_security_set_file_context(const char *path, mkrn_sec_id_t sid);

/**
 * mkrn_security_get_file_context - Get file security context
 * @path: File path
 *
 * Return: Security ID
 */
mkrn_sec_id_t mkrn_security_get_file_context(const char *path);

/**
 * mkrn_security_audit_log - Log a security audit event
 * @operation: Operation name
 * @sid: Security ID
 * @object: Object name
 * @allowed: Whether access was allowed
 *
 * Return: void
 */
void mkrn_security_audit_log(const char *operation, mkrn_sec_id_t sid,
                             const char *object, b allowed);

/**
 * mkrn_security_print_status - Print security status
 *
 * Return: void
 */
void mkrn_security_print_status(void);

/**
 * mkrn_security_is_enabled - Check if security is enabled
 *
 * Return: True if enabled
 */
b mkrn_security_is_enabled(void);

/**
 * mkrn_security_check_memory_access - Check memory access permission
 * @sid: Source security ID
 * @addr: Memory address
 * @size: Access size
 * @permission: Requested permission
 *
 * Return: 0 if allowed, -1 if denied
 */
int mkrn_security_check_memory_access(mkrn_sec_id_t sid, void *addr, size_t size, u32 permission);

/**
 * mkrn_security_set_memory_protection - Set memory protection
 * @addr: Memory address
 * @size: Region size
 * @permission: Permission bitmask
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_security_set_memory_protection(void *addr, size_t size, u32 permission);

/**
 * mkrn_security_check_socket_access - Check socket access permission
 * @sid: Source security ID
 * @domain: Socket domain
 * @type: Socket type
 * @protocol: Socket protocol
 *
 * Return: 0 if allowed, -1 if denied
 */
int mkrn_security_check_socket_access(mkrn_sec_id_t sid, int domain, int type, int protocol);

/**
 * mkrn_security_check_packet_access - Check packet access permission
 * @sid: Source security ID
 * @src_ip: Source IP
 * @dst_ip: Destination IP
 * @port: Port number
 *
 * Return: 0 if allowed, -1 if denied
 */
int mkrn_security_check_packet_access(mkrn_sec_id_t sid, u32 src_ip, u32 dst_ip, u16 port);

/**
 * mkrn_security_check_process_create - Check process creation permission
 * @sid: Source security ID
 * @name: Process name
 *
 * Return: 0 if allowed, -1 if denied
 */
int mkrn_security_check_process_create(mkrn_sec_id_t sid, const char *name);

/**
 * mkrn_security_check_process_transition - Check process transition permission
 * @sid: Source security ID
 * @tsid: Target security ID
 *
 * Return: 0 if allowed, -1 if denied
 */
int mkrn_security_check_process_transition(mkrn_sec_id_t sid, mkrn_sec_id_t tsid);

/**
 * mkrn_security_load_policy - Load a security policy from file
 * @policy_file: Policy file path
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_security_load_policy(const char *policy_file);

/**
 * mkrn_security_save_policy - Save security policy to file
 * @policy_file: Policy file path
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_security_save_policy(const char *policy_file);

/**
 * mkrn_security_compute_access_vector - Compute access vector
 * @ssid: Source security ID
 * @tsid: Target security ID
 * @object_class: Object class name
 * @requested: Requested permission bitmask
 *
 * Return: Computed access vector
 */
int mkrn_security_compute_access_vector(mkrn_sec_id_t ssid, mkrn_sec_id_t tsid,
                                        const char *object_class, u32 requested);

typedef struct {
    u64 access_checks;
    u64 granted_access;
    u64 denied_access;
    u64 policy_loads;
    u64 context_transitions;
} mkrn_sec_stats_t;

/**
 * mkrn_security_get_stats - Get security statistics
 * @stats: Structure to fill
 *
 * Return: void
 */
void mkrn_security_get_stats(mkrn_sec_stats_t *stats);

typedef int (*mkrn_sec_hook_t)(void *arg1, void *arg2, void *arg3);

/**
 * mkrn_security_register_hook - Register a security hook
 * @name: Hook name
 * @hook: Hook function pointer
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_security_register_hook(const char *name, mkrn_sec_hook_t hook);

/**
 * mkrn_security_unregister_hook - Unregister a security hook
 * @name: Hook name
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_security_unregister_hook(const char *name);

/**
 * mkrn_security_set_label - Set a security label on a path
 * @path: File path
 * @label: Security label
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_security_set_label(const char *path, const char *label);

/**
 * mkrn_security_get_label - Get security label for a path
 * @path: File path
 *
 * Return: Label string, NULL on failure
 */
char *mkrn_security_get_label(const char *path);

/**
 * mkrn_security_compile_policy - Compile a security policy
 * @source_policy: Source policy file
 * @binary_policy: Output binary policy file
 *
 * Return: 0 on success, -1 on failure
 */
int mkrn_security_compile_policy(const char *source_policy, const char *binary_policy);

/**
 * mkrn_security_validate_policy - Validate a security policy file
 * @policy_file: Policy file path
 *
 * Return: 0 if valid, -1 if invalid
 */
int mkrn_security_validate_policy(const char *policy_file);
