/*
 * M4KK1 4P1 - stdarg.h
 * Description: Variable argument list definitions.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

typedef char *va_list;

#define va_start(ap, last)  (ap = (va_list)&last + sizeof(last))
#define va_arg(ap, type)    (*(type *)((ap += sizeof(type)) - sizeof(type)))
#define va_end(ap)          (ap = (va_list)0)
#define va_copy(dest, src)  (dest = src)
