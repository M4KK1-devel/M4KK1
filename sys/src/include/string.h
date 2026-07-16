/*
 * M4KK1 4P1 - string.h
 * Description: String and memory operation declarations.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stddef.h>

#define memcpy  mkrn_memcpy
#define memset  mkrn_memset
#define memcmp  mkrn_memcmp
#define memmove mkrn_memmove
#define memchr  mkrn_memchr
#define strlen  mkrn_strlen
#define strcpy  mkrn_strcpy
#define strncpy mkrn_strncpy
#define strcmp  mkrn_strcmp
#define strdup  mkrn_strdup

/**
 * mkrn_memcpy - Copy memory block
 * @dest: Destination pointer
 * @src: Source pointer
 * @n: Number of bytes
 *
 * Return: Pointer to destination
 */
void *mkrn_memcpy(void *dest, const void *src, size_t n);

/**
 * mkrn_memset - Set memory block
 * @s: Memory pointer
 * @c: Value to set
 * @n: Number of bytes
 *
 * Return: Pointer to memory
 */
void *mkrn_memset(void *s, int c, size_t n);

/**
 * mkrn_memcmp - Compare memory blocks
 * @s1: First block
 * @s2: Second block
 * @n: Number of bytes
 *
 * Return: 0 if equal, difference otherwise
 */
int mkrn_memcmp(const void *s1, const void *s2, size_t n);

/**
 * mkrn_memmove - Move memory block
 * @dest: Destination
 * @src: Source
 * @n: Number of bytes
 *
 * Return: Pointer to destination
 */
void *mkrn_memmove(void *dest, const void *src, size_t n);

/**
 * mkrn_memchr - Find character in memory
 * @s: Memory pointer
 * @c: Character to find
 * @n: Number of bytes
 *
 * Return: Pointer to character, NULL if not found
 */
void *mkrn_memchr(const void *s, int c, size_t n);

/**
 * mkrn_strlen - Get string length
 * @s: Null-terminated string
 *
 * Return: Length of string
 */
size_t mkrn_strlen(const char *s);

/**
 * mkrn_strcpy - Copy string
 * @dest: Destination buffer
 * @src: Source string
 *
 * Return: Pointer to destination
 */
char *mkrn_strcpy(char *dest, const char *src);

/**
 * mkrn_strcat - Concatenate strings
 * @dest: Destination buffer
 * @src: Source string
 *
 * Return: Pointer to destination
 */
char *mkrn_strcat(char *dest, const char *src);

/**
 * mkrn_strcmp - Compare strings
 * @s1: First string
 * @s2: Second string
 *
 * Return: 0 if equal, difference otherwise
 */
int mkrn_strcmp(const char *s1, const char *s2);

/**
 * mkrn_strncpy - Copy string with length limit
 * @dest: Destination buffer
 * @src: Source string
 * @n: Maximum length
 *
 * Return: Pointer to destination
 */
char *mkrn_strncpy(char *dest, const char *src, size_t n);

/**
 * mkrn_strncat - Concatenate strings with length limit
 * @dest: Destination buffer
 * @src: Source string
 * @n: Maximum length
 *
 * Return: Pointer to destination
 */
char *mkrn_strncat(char *dest, const char *src, size_t n);

/**
 * mkrn_strncmp - Compare strings with length limit
 * @s1: First string
 * @s2: Second string
 * @n: Maximum length
 *
 * Return: 0 if equal, difference otherwise
 */
int mkrn_strncmp(const char *s1, const char *s2, size_t n);

/**
 * mkrn_strchr - Find character in string
 * @s: String to search
 * @c: Character to find
 *
 * Return: Pointer to character, NULL if not found
 */
char *mkrn_strchr(const char *s, int c);

/**
 * mkrn_strstr - Find substring in string
 * @haystack: String to search
 * @needle: Substring to find
 *
 * Return: Pointer to substring, NULL if not found
 */
char *mkrn_strstr(const char *haystack, const char *needle);

/**
 * mkrn_strdup - Duplicate a string
 * @s: String to duplicate
 *
 * Return: Pointer to new string, NULL on failure
 */
char *mkrn_strdup(const char *s);
