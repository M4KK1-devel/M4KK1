/*
 * M4KK1 4P1 - stdio.c
 * Description: Minimal stdio implementation for M4KK1
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "include/stdio.h"
#include "include/stdlib.h"
#include "include/string.h"
#include "include/unistd.h"
#include "include/errno.h"

/* Standard streams */
static FILE _stdin = {0, 0, NULL, 0, 0, 0, 0};
static FILE _stdout = {1, 0, NULL, 0, 0, 0, 0};
static FILE _stderr = {2, 0, NULL, 0, 0, 0, 0};

FILE *stdin = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

FILE *fopen(const char *path, const char *mode)
{
    int flags = 0;
    if (mode[0] == 'r') flags = 0x01; /* O_RDONLY */
    else if (mode[0] == 'w') flags = 0x02 | 0x0100 | 0x1000; /* O_WRONLY | O_CREAT | O_TRUNC */
    else if (mode[0] == 'a') flags = 0x02 | 0x0100 | 0x2000; /* O_WRONLY | O_CREAT | O_APPEND */

    if (mode[1] == '+') {
        flags = (flags & ~0x03) | 0x04; /* O_RDWR */
    }

    int fd = open(path, flags, 0644);
    if (fd < 0) return NULL;

    FILE *f = (FILE *)malloc(sizeof(FILE));
    if (!f) {
        close(fd);
        return NULL;
    }

    f->fd = fd;
    f->flags = flags;
    f->buf = NULL;
    f->buf_size = 0;
    f->buf_pos = 0;
    f->eof = 0;
    f->error = 0;

    return f;
}

int fclose(FILE *stream)
{
    if (!stream) return -1;
    fflush(stream);
    if (stream->buf) free(stream->buf);
    int ret = close(stream->fd);
    if (stream != stdin && stream != stdout && stream != stderr)
        free(stream);
    return ret;
}

int fflush(FILE *stream)
{
    if (!stream || !stream->buf || stream->buf_pos == 0) return 0;
    ssize_t written = write(stream->fd, stream->buf, stream->buf_pos);
    if (written < 0) {
        stream->error = 1;
        return -1;
    }
    stream->buf_pos = 0;
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (!stream || !ptr) return 0;
    size_t total = size * nmemb;
    ssize_t n = read(stream->fd, ptr, total);
    if (n <= 0) {
        if (n == 0) stream->eof = 1;
        else stream->error = 1;
        return 0;
    }
    return n / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    if (!stream || !ptr) return 0;
    size_t total = size * nmemb;
    ssize_t n = write(stream->fd, ptr, total);
    if (n < 0) {
        stream->error = 1;
        return 0;
    }
    return n / size;
}

int fgetc(FILE *stream)
{
    unsigned char c;
    if (fread(&c, 1, 1, stream) != 1) return -1;
    return c;
}

int fputc(int c, FILE *stream)
{
    unsigned char ch = c;
    if (fwrite(&ch, 1, 1, stream) != 1) return -1;
    return c;
}

char *fgets(char *s, int size, FILE *stream)
{
    if (!s || size <= 0 || !stream) return NULL;
    int i = 0;
    while (i < size - 1) {
        int c = fgetc(stream);
        if (c < 0) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

int fputs(const char *s, FILE *stream)
{
    size_t len = strlen(s);
    if (fwrite(s, 1, len, stream) != len) return -1;
    return 0;
}

int getchar(void) { return fgetc(stdin); }
int putchar(int c) { return fputc(c, stdout); }
int puts(const char *s) {
    if (fputs(s, stdout) < 0) return -1;
    return fputc('\n', stdout);
}

int printf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stdout, format, ap);
    va_end(ap);
    return ret;
}

int fprintf(FILE *stream, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

int sprintf(char *str, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vsprintf(str, format, ap);
    va_end(ap);
    return ret;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

/* Simplified vsnprintf implementation */
int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    if (!str || size == 0) return 0;
    size_t pos = 0;
    while (*format && pos < size - 1) {
        if (*format == '%') {
            format++;
            if (*format == 'd') {
                int val = va_arg(ap, int);
                char buf[32];
                int len = 0;
                if (val < 0) {
                    if (pos < size - 1) str[pos++] = '-';
                    val = -val;
                }
                do {
                    buf[len++] = '0' + (val % 10);
                    val /= 10;
                } while (val > 0 && len < 31);
                while (len > 0 && pos < size - 1) {
                    str[pos++] = buf[--len];
                }
            } else if (*format == 's') {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s && pos < size - 1) {
                    str[pos++] = *s++;
                }
            } else if (*format == 'c') {
                int c = va_arg(ap, int);
                if (pos < size - 1) str[pos++] = c;
            } else if (*format == '%') {
                if (pos < size - 1) str[pos++] = '%';
            }
            format++;
        } else {
            str[pos++] = *format++;
        }
    }
    str[pos] = '\0';
    return pos;
}

int vfprintf(FILE *stream, const char *format, va_list ap)
{
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    if (len > 0) {
        fwrite(buf, 1, len, stream);
    }
    return len;
}

int vsprintf(char *str, const char *format, va_list ap)
{
    return vsnprintf(str, 4096, format, ap);
}

int fseek(FILE *stream, long offset, int whence)
{
    if (!stream) return -1;
    off_t ret = lseek(stream->fd, offset, whence);
    if (ret < 0) return -1;
    stream->eof = 0;
    return 0;
}

long ftell(FILE *stream)
{
    if (!stream) return -1;
    return lseek(stream->fd, 0, SEEK_CUR);
}

void rewind(FILE *stream)
{
    fseek(stream, 0, SEEK_SET);
    stream->eof = 0;
}

int feof(FILE *stream) { return stream ? stream->eof : 0; }
int ferror(FILE *stream) { return stream ? stream->error : 0; }
void clearerr(FILE *stream) {
    if (stream) {
        stream->eof = 0;
        stream->error = 0;
    }
}

int remove(const char *path) { return unlink(path); }
int rename(const char *oldpath, const char *newpath) {
    /* M4KK1 syscall would go here */
    return -1;
}
