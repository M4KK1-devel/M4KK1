/*
 * M4KK1 Variable Arguments
 * stdarg.h - Variable argument list handling
 */

#ifndef _STDARG_H
#define _STDARG_H

/* GCC built-in va_list type */
typedef __builtin_va_list va_list;

/* GCC built-in macros */
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif /* _STDARG_H */
