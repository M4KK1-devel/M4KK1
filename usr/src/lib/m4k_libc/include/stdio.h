/*
 * M4KK1 4P1 - stdio.h
 * Description: Minimal stdio implementation for M4KK1
 * Maps standard C I/O to M4KK1 system calls
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#ifndef _M4K_STDIO_H
#define _M4K_STDIO_H

#include <stddef.h>
#include <stdarg.h>

/* File descriptor limits */
#define FOPEN_MAX 16
#define FILENAME_MAX 256
#define BUFSIZ 512

/* File structure */
typedef struct {
    int fd;
    int flags;
    char *buf;
    size_t buf_size;
    size_t buf_pos;
    int eof;
    int error;
} FILE;

/* Standard streams */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* File operations */
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fflush(FILE *stream);
FILE *freopen(const char *path, const char *mode, FILE *stream);

/* Reading */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
int fgetc(FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int getchar(void);
char *gets(char *s);

/* Writing */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int putchar(int c);
int puts(const char *s);

/* Formatted I/O */
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vprintf(const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsprintf(char *str, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

/* Scanning */
int scanf(const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);
int sscanf(const char *str, const char *format, ...);

/* File positioning */
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int fgetpos(FILE *stream, long *pos);
int fsetpos(FILE *stream, const long *pos);

/* Error handling */
void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void perror(const char *s);

/* File removal and renaming */
int remove(const char *path);
int rename(const char *oldpath, const char *newpath);

/* Temporary files */
FILE *tmpfile(void);
char *tmpnam(char *s);

#endif /* _M4K_STDIO_H */
