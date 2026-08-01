/*
 * M4KK1 Standard Definitions
 * stddef.h - Standard type definitions
 */

#ifndef _STDDEF_H
#define _STDDEF_H

/* NULL pointer */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* Size type */
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned long size_t;
#endif

/* Signed size type */
#ifndef _SSIZE_T
#define _SSIZE_T
typedef long ssize_t;
#endif

/* Pointer difference type */
#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef long ptrdiff_t;
#endif

/* Wide character type */
#ifndef _WCHAR_T
#define _WCHAR_T
typedef int wchar_t;
#endif

/* Offset type */
#ifndef _OFF_T
#define _OFF_T
typedef long off_t;
#endif

/* Maximum offset type */
#ifndef _OFF64_T
#define _OFF64_T
typedef long long off64_t;
#endif

#endif /* _STDDEF_H */
