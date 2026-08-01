/*
 * M4KK1 4P1 - ccconfig.h
 * Description: PCC configuration for M4KK1 OS
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

/*
 * Various settings that controls how the C compiler works.
 */

/* common cpp predefines */
#define CPPADD	{ "-D__m4kk1__", "-D__ELF__", "-D__i386__", NULL, }

#define CRT0		"crt0.o"
#define GCRT0		"gcrt0.o"

#define STARTLABEL "_start"

/* M4KK1 uses static linking only */
#define DYNLINKLIB	NULL

#define CPPMDADD	{ "-D__i386__", NULL, }

/* No dynamic linking support */
#undef USE_MUSL
