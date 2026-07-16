/*
 * M4KK1 4P1 - selinux.c
 * Description: SELinux-style security framework —
 *              context management, policy rules, access
 *              control, and audit logging.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "../include/security.h"
#include "../../include/console.h"
#include "../../include/memory.h"
#include "../../include/string.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char user[64];
    char role[64];
    char type[64];
    char level[64];
} sec_context_t;

typedef struct sec_rule {
    char source_type[64];
    char target_type[64];
    char object_class[64];
    uint32_t permissions;
    struct sec_rule *next;
} sec_rule_t;

typedef struct sec_sid {
    uint32_t sid;
    sec_context_t context;
    struct sec_sid *next;
} sec_sid_t;

static struct {
    bool bEnabled;
    bool bEnforcing;
    sec_sid_t *pSidList;
    sec_rule_t *pPolicyRules;
    uint32_t u32NextSid;
} sec_state;

#define SEC_READ    (1 << 0)
#define SEC_WRITE   (1 << 1)
#define SEC_EXECUTE (1 << 2)
#define SEC_CREATE  (1 << 3)
#define SEC_DELETE  (1 << 4)
#define SEC_IOCTL   (1 << 5)
#define SEC_LOCK    (1 << 6)
#define SEC_LINK    (1 << 7)

#define SEC_CLASS_FILE    1
#define SEC_CLASS_DIR     2
#define SEC_CLASS_SOCKET  3
#define SEC_CLASS_PROCESS 4
#define SEC_CLASS_SYSTEM  5

static bool
sec_check_permission(uint32_t u32Sid,
                     uint32_t u32Tsid,
                     uint32_t u32ObjectClass,
                     uint32_t u32Permission)
{
    if (!sec_state.bEnforcing)
        return true;

    sec_rule_t *pRule = sec_state.pPolicyRules;
    while (pRule) {
        if (mkrn_strcmp(pRule->source_type,
                   "unconfined_t") == 0
            && pRule->object_class
                   == u32ObjectClass)
        {
            if (pRule->permissions & u32Permission)
                return true;
        }
        pRule = pRule->next;
    }

    return false;
}

static uint32_t
sec_alloc_sid(sec_context_t *pContext)
{
    sec_sid_t *pSid =
        (sec_sid_t *)kmalloc(sizeof(sec_sid_t));
    if (!pSid)
        return 0;

    pSid->sid = sec_state.u32NextSid++;
    mkrn_memcpy(&pSid->context, pContext,
           sizeof(sec_context_t));
    pSid->next = sec_state.pSidList;
    sec_state.pSidList = pSid;

    return pSid->sid;
}

static sec_sid_t *
sec_find_sid(uint32_t u32Sid)
{
    sec_sid_t *pCurrent = sec_state.pSidList;
    while (pCurrent) {
        if (pCurrent->sid == u32Sid)
            return pCurrent;
        pCurrent = pCurrent->next;
    }
    return NULL;
}

/**
 * @brief  Initialize the security framework.
 */
int
mkrn_sec_init(void)
{
    mkrn_console_write(
        "Initializing M4KK1 Security Framework...\n");

    mkrn_memset(&sec_state, 0, sizeof(sec_state));
    sec_state.bEnabled = true;
    sec_state.bEnforcing = false;
    sec_state.u32NextSid = 1;

    sec_context_t kernel_ctx = {
        .user = "system_u",
        .role = "system_r",
        .type = "kernel_t",
        .level = "s0"
    };

    sec_alloc_sid(&kernel_ctx);

    mkrn_console_write(
        "Security framework initialized\n");
    return 0;
}

/**
 * @brief  Set the security mode (enforcing / permissive).
 */
void
mkrn_sec_set_mode(bool bEnforcing)
{
    sec_state.bEnforcing = bEnforcing;

    mkrn_console_write("Security mode set to: ");
    mkrn_console_write(
        bEnforcing ? "ENFORCING" : "PERMISSIVE");
    mkrn_console_write("\n");
}

/**
 * @brief  Get the current security mode.
 */
bool
mkrn_sec_get_mode(void)
{
    return sec_state.bEnforcing;
}

/**
 * @brief  Create a security context and return its SID.
 */
uint32_t
mkrn_sec_create_context(const char *pUser,
                        const char *pRole,
                        const char *pType,
                        const char *pLevel)
{
    sec_context_t ctx;

    mkrn_strncpy(ctx.user, pUser, sizeof(ctx.user) - 1);
    mkrn_strncpy(ctx.role, pRole, sizeof(ctx.role) - 1);
    mkrn_strncpy(ctx.type, pType, sizeof(ctx.type) - 1);
    mkrn_strncpy(ctx.level, pLevel,
            sizeof(ctx.level) - 1);

    return sec_alloc_sid(&ctx);
}

/**
 * @brief  Destroy a security context by SID.
 */
void
mkrn_sec_destroy_context(uint32_t u32Sid)
{
    sec_sid_t *pCurrent = sec_state.pSidList;
    sec_sid_t *pPrev = NULL;

    while (pCurrent) {
        if (pCurrent->sid == u32Sid) {
            if (pPrev)
                pPrev->next = pCurrent->next;
            else
                sec_state.pSidList =
                    pCurrent->next;
            kfree(pCurrent);
            return;
        }
        pPrev = pCurrent;
        pCurrent = pCurrent->next;
    }
}

/**
 * @brief  Add a security policy rule.
 */
int
mkrn_sec_add_rule(const char *pSourceType,
                  const char *pTargetType,
                  const char *pObjectClass,
                  uint32_t u32Permissions)
{
    sec_rule_t *pRule = (sec_rule_t *)kmalloc(
        sizeof(sec_rule_t));
    if (!pRule)
        return -1;

    mkrn_strncpy(pRule->source_type, pSourceType,
            sizeof(pRule->source_type) - 1);
    mkrn_strncpy(pRule->target_type, pTargetType,
            sizeof(pRule->target_type) - 1);
    mkrn_strncpy(pRule->object_class, pObjectClass,
            sizeof(pRule->object_class) - 1);
    pRule->permissions = u32Permissions;
    pRule->next = sec_state.pPolicyRules;
    sec_state.pPolicyRules = pRule;

    mkrn_console_write("Security rule added: ");
    mkrn_console_write(pSourceType);
    mkrn_console_write(" -> ");
    mkrn_console_write(pTargetType);
    mkrn_console_write(" (");
    mkrn_console_write(pObjectClass);
    mkrn_console_write(")\n");

    return 0;
}

/**
 * @brief  Check whether an access is allowed.
 */
bool
mkrn_sec_check_access(uint32_t u32Sid,
                      uint32_t u32Tsid,
                      const char *pObjectClass,
                      uint32_t u32Permission)
{
    if (!sec_state.bEnabled)
        return true;

    sec_sid_t *pSourceSid = sec_find_sid(u32Sid);
    sec_sid_t *pTargetSid = sec_find_sid(u32Tsid);

    if (!pSourceSid || !pTargetSid)
        return false;

    uint32_t u32ClassId = 0;
    if (mkrn_strcmp(pObjectClass, "file") == 0)
        u32ClassId = SEC_CLASS_FILE;
    else if (mkrn_strcmp(pObjectClass, "dir") == 0)
        u32ClassId = SEC_CLASS_DIR;
    else if (mkrn_strcmp(pObjectClass, "socket") == 0)
        u32ClassId = SEC_CLASS_SOCKET;
    else if (mkrn_strcmp(pObjectClass, "process") == 0)
        u32ClassId = SEC_CLASS_PROCESS;
    else
        u32ClassId = SEC_CLASS_SYSTEM;

    bool bAllowed = sec_check_permission(
        u32Sid, u32Tsid, u32ClassId,
        u32Permission);

    if (!bAllowed && sec_state.bEnforcing) {
        mkrn_console_write(
            "SECURITY DENIED: ");
        mkrn_console_write(
            pSourceSid->context.type);
        mkrn_console_write(" -> ");
        mkrn_console_write(
            pTargetSid->context.type);
        mkrn_console_write(" (");
        mkrn_console_write(pObjectClass);
        mkrn_console_write(")\n");
    }

    return bAllowed;
}

/**
 * @brief  Get the current process security context (SID).
 */
uint32_t
mkrn_sec_get_process_context(void)
{
    return 1;
}

/**
 * @brief  Set the security context for a file.
 */
int
mkrn_sec_set_file_context(const char *pPath,
                          uint32_t u32Sid)
{
    (void)u32Sid;
    mkrn_console_write(
        "Setting security context for: ");
    mkrn_console_write(pPath);
    mkrn_console_write("\n");
    return 0;
}

/**
 * @brief  Get the security context for a file (SID).
 */
uint32_t
mkrn_sec_get_file_context(const char *pPath)
{
    (void)pPath;
    return 1;
}

/**
 * @brief  Log a security audit record.
 */
void
mkrn_sec_audit_log(const char *pOperation,
                   uint32_t u32Sid,
                   const char *pObject,
                   bool bAllowed)
{
    if (!sec_state.bEnabled)
        return;

    mkrn_console_write("SECURITY AUDIT: ");
    mkrn_console_write(pOperation);
    mkrn_console_write(" on ");
    mkrn_console_write(pObject);
    mkrn_console_write(" - ");
    mkrn_console_write(
        bAllowed ? "ALLOWED" : "DENIED");
    mkrn_console_write("\n");
}

/**
 * @brief  Print the current security framework status.
 */
void
mkrn_sec_print_status(void)
{
    mkrn_console_write(
        "=== M4KK1 Security Framework Status ===\n");
    mkrn_console_write("Enabled: ");
    mkrn_console_write(
        sec_state.bEnabled ? "YES" : "NO");
    mkrn_console_write("\n");
    mkrn_console_write("Mode: ");
    mkrn_console_write(
        sec_state.bEnforcing
            ? "ENFORCING" : "PERMISSIVE");
    mkrn_console_write("\n");
    mkrn_console_write("Next SID: ");
    mkrn_console_write_dec(
        sec_state.u32NextSid);
    mkrn_console_write("\n");
    mkrn_console_write("Rules count: ");

    uint32_t u32Count = 0;
    sec_rule_t *pRule = sec_state.pPolicyRules;
    while (pRule) {
        u32Count++;
        pRule = pRule->next;
    }
    mkrn_console_write_dec(u32Count);
    mkrn_console_write("\n");
    mkrn_console_write(
        "=======================================\n");
}

/**
 * @brief  Load the default security policy.
 */
void
mkrn_sec_load_default_policy(void)
{
    mkrn_console_write(
        "Loading default security policy...\n");

    mkrn_sec_add_rule(
        "unconfined_t", "unconfined_t", "file",
        SEC_READ | SEC_WRITE | SEC_EXECUTE);
    mkrn_sec_add_rule(
        "unconfined_t", "unconfined_t", "dir",
        SEC_READ | SEC_WRITE | SEC_EXECUTE);
    mkrn_sec_add_rule(
        "unconfined_t", "unconfined_t", "socket",
        SEC_READ | SEC_WRITE);
    mkrn_sec_add_rule(
        "unconfined_t", "unconfined_t", "process",
        SEC_READ | SEC_WRITE | SEC_EXECUTE);

    mkrn_console_write(
        "Default security policy loaded\n");
}

/**
 * @brief  Clean up the security framework.
 */
void
mkrn_sec_cleanup(void)
{
    mkrn_console_write(
        "Cleaning up security framework...\n");

    sec_sid_t *pSid = sec_state.pSidList;
    while (pSid) {
        sec_sid_t *pNext = pSid->next;
        kfree(pSid);
        pSid = pNext;
    }

    sec_rule_t *pRule = sec_state.pPolicyRules;
    while (pRule) {
        sec_rule_t *pNext = pRule->next;
        kfree(pRule);
        pRule = pNext;
    }

    mkrn_memset(&sec_state, 0, sizeof(sec_state));

    mkrn_console_write(
        "Security framework cleanup completed\n");
}
