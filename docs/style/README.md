# M4KK1 4P1 Coding Style Specification (v1.0)
**"Readability by Convention. Consistency by Force."**

---

## 0. Core Principles

1. **Convention over configuration**: Write strictly to this spec.
   No extra formatting tooling required for human readability.
2. **Explicit over implicit**: Use naming and prefixes to clearly
   identify scope and type.
3. **Minimalism**: Deep nesting (pointer depth and logic branching)
   is forbidden.

---

## 1. Basic Formatting

### 1.1 Indentation and Line Width

- **Indent**: 4 spaces (no tab characters).
- **Line width**: Maximum **80 characters**.
- **Wrapping**: When a line exceeds 80 chars, break after an operator
  (`+`, `&&`, `||`, etc.) and indent the continuation by 8 spaces.

### 1.2 Brace Style (K&R)

**Control flow**: Opening brace `{` stays on the same line. Closing
brace `}` is on its own line.

```c
/* Correct (K&R) */
if (condition) {
    do_something();
} else {
    do_other();
}

/* Wrong (Allman) */
if (condition)
{
    do_something();
}
```

**Exception -- function definitions**: The opening brace goes on a
**new line**.

```c
/* Function definition: brace on new line */
int mkrn_schedule(void)
{
    return 0;
}
```

---

## 2. Naming Convention

Uses a **Hungarian + camelCase** hybrid style, strictly
distinguishing **kernel space** from **user space**.

### 2.1 Scope and Ownership Prefixes

| Prefix | Full Name | Scope | Example |
| :--- | :--- | :--- | :--- |
| `mkrn_` | M4K KeRnel | **Kernel source** (`sys/src/` all files) | `mkrn_page_alloc()`, `mkrn_current_task` |
| `musr_` | M4K USeR | **User-space** (`usr/bin/`, `lib/`) | `musr_printf()`, `musr_strlen` |
| (none) | local | **File-static or local** variables | `int total; char *buf;` |

> **Mandatory**: Every exported global function or variable **must**
> carry `mkrn_` or `musr_`. No prefix means it is `static` to that
> translation unit.

**Note on `m4k_`**: The `m4k_` prefix is reserved for the **syscall
ABI layer** (declared in `<m4k/syscall.h>`) and is the only exception
to the `mkrn_`/`musr_` rule.

### 2.2 Hungarian Type Tags

Append a type tag after the prefix (and before the camelCase name).

| Tag | Meaning | Example |
| :--- | :--- | :--- |
| `u32` | uint32_t | `mkrn_u32TickCount` |
| `s64` | int64_t | `mkrn_s64Offset` |
| `p` | Pointer | `pCharBuffer` (`char*`), `pM4kTask` |
| `sz` | Null-terminated string | `szPathname` |
| `b` | Boolean | `bIsReady` |
| `e` | Enum | `eFileMode` |
| `fn` | Function pointer | `fnCallback` |

**Examples**:
- Kernel global function: `mkrn_schedule()` (no type tag, just
  camelCase).
- Kernel global variable: `mkrn_u32TickCount`.
- User-space static function: `static int musr_format_buffer()`.
- User-space local pointer: `char *pBuf`.

### 2.3 Macros and Constants

- **Macros**: ALL_CAPS + underscore.
```c
#define M4K_SYS_OPEN  0xM4K00010
#define SEAD_OPS_MAX  64
```
- **Enum values**: ALL_CAPS + underscore.
```c
enum file_flags {
    FILE_FLAG_READ  = 0x01,
    FILE_FLAG_WRITE = 0x02
};
```

---

## 3. Pointer Restrictions

**Hard limit**: Pointer nesting must not exceed **2 levels**
(i.e., `***` is strictly forbidden).

| Level | Example | Allowed? | Alternative |
| :--- | :--- | :--- | :--- |
| 0 | `int value` | Yes | |
| 1 | `int *pValue` | Yes | |
| 2 | `char **argv` | Yes (function params only) | |
| 3+ | `void ***pppData` | **No** | Use `struct` + double pointer |

**Additional rules for double pointers**:
- Only **function parameters** may use `char **argv`.
- Struct members must not be double pointers (prevents leaks and
  dangling references).

---

## 4. Comment Style

### 4.1 File Header

Every `.c` / `.h` file must begin with:

```c
/*
 * M4KK1 4P1 - filename.c
 * Description: Brief description of purpose.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */
```

### 4.2 Function Documentation (Doxygen)

Every exported (`mkrn_` / `musr_` / `m4k_`) function must have:

```c
/**
 * mkrn_page_alloc - Allocate physical memory pages
 * @order: Number of contiguous pages (2^order)
 *
 * Return: Physical address, or NULL_PHYS on failure
 */
uintptr_t mkrn_page_alloc(int order);
```

### 4.3 Inline Comments

- Use `/* comment */` format only. Do **not** use `//` comments.
- Write comments to explain **why**, not **what**.

---

## 5. Headers and Includes

### 5.1 Header Guard

Use `#pragma once` (supported by GCC and Clang).

```c
#pragma once
```

### 5.2 Include Order (Strict)

In every `.c` file, `#include` directives must appear in this order:

1. **Corresponding header** (e.g., `src.c` first includes
   `"src.h"`).
2. **Kernel-wide headers** (`<m4k/types.h>`).
3. **Library headers** (alphabetical, e.g., `<string.h>`).
4. **Local headers** (`"local_dep.h"`).

---

## 6. Error Handling

- Every `mkrn_` function must return values per the 4P1 error
  convention: 0 or positive on success, negative `M4K_E*` on failure.
- Do **not** call `panic()` inside functions (reserve for
  unrecoverable hardware faults).
- Callers must check return values unless marked `__must_check`.

```c
int ret = mkrn_open(szPath, flags);
if (ret < 0) {
    return ret;
}
```

---

## 7. Hard Limits Quick Reference

| Item | Limit |
| :--- | :--- |
| Pointer nesting | Max **2 levels** (`**`) |
| Function length | Max **100 lines** (40 recommended) |
| Branch nesting | Max **3 levels** (`if`/`for`/`while`) |
| Global variables | Must have `mkrn_`/`musr_` prefix; declare `extern` in header |
| Casts | Explicit casts **must** be followed by a comment explaining safety |

---

## 8. Good Code vs. Bad Code

### Correct (4P1-compliant)

```c
/*
 * M4KK1 4P1 - kernel/vm/mkrn_page.c
 * Description: Physical page frame allocator core.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once
#include <m4k/types.h>

/* Global kernel variable */
uint32_t mkrn_u32FreePages = 0;

/**
 * mkrn_alloc_frame - Allocate one physical page
 *
 * Return: Physical address, or M4K_ENOMEM on error
 */
uintptr_t mkrn_alloc_frame(void)
{
    uint32_t u32Frame = mkrn_find_free();
    if (u32Frame == INVALID) {
        return -M4K_ENOMEM;
    }
    mkrn_u32FreePages--;
    return (uintptr_t)(u32Frame << 12);
}
```

### Wrong (not 4P1-compliant)

```c
// ERROR 1: tab indent, missing prefix
void my_malloc() {
    int ***pData; /* ERROR 2: triple pointer */
    // ERROR 3: no Hungarian notation
    int a;
}
```
