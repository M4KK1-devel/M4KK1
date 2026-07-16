/*
 * M4KK1 4P1 - stddef.h
 * Description: Standard definitions and types.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#define NULL ((void *)0)

typedef unsigned int size_t;
typedef int ptrdiff_t;
typedef int wchar_t;

#define offsetof(type, member) ((size_t)&((type *)0)->member)
