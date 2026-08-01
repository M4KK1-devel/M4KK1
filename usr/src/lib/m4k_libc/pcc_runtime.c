/*
 * M4KK1 PCC Runtime Library
 * pcc_runtime.c - Compiler runtime support functions
 */

#include <stdint.h>

/* 64-bit division support */
int64_t __divdi3(int64_t a, int64_t b) {
    return a / b;
}

int64_t __moddi3(int64_t a, int64_t b) {
    return a % b;
}

uint64_t __udivdi3(uint64_t a, uint64_t b) {
    return a / b;
}

uint64_t __umoddi3(uint64_t a, uint64_t b) {
    return a % b;
}

/* 64-bit multiplication support */
int64_t __muldi3(int64_t a, int64_t b) {
    return a * b;
}

/* Division and modulo */
void __divmoddi4(int64_t a, int64_t b, int64_t *q, int64_t *r) {
    *q = a / b;
    *r = a % b;
}

/* Shift operations */
int64_t __ashldi3(int64_t a, int b) {
    return a << b;
}

int64_t __ashrdi3(int64_t a, int b) {
    return a >> b;
}

uint64_t __lshrdi3(uint64_t a, int b) {
    return a >> b;
}

/* Comparison operations */
int __cmpdi2(int64_t a, int64_t b) {
    if (a < b) return 0;
    if (a > b) return 2;
    return 1;
}

int __ucmpdi2(uint64_t a, uint64_t b) {
    if (a < b) return 0;
    if (a > b) return 2;
    return 1;
}

/* Floating point support */
double __floatdidf(int64_t a) {
    return (double)a;
}

float __floatdisf(int64_t a) {
    return (float)a;
}

int64_t __fixdfdi(double a) {
    return (int64_t)a;
}

int64_t __fixsfdi(float a) {
    return (int64_t)a;
}

uint64_t __fixunsdfdi(double a) {
    return (uint64_t)a;
}

uint64_t __fixunssfdi(float a) {
    return (uint64_t)a;
}
