/*
 * mkrn_kstrtox.h — 内核字符串→整数转换接口（4P1 移植层）
 *
 * M4KK1 4P1 / 2026-08
 */

#pragma once

#include <stdint.h>

int mkrn_kstrtoull(const char *s, unsigned int base,
                   uint64_t *res);
int mkrn_kstrtoll(const char *s, unsigned int base,
                  int64_t *res);
int mkrn_kstrtoul(const char *s, unsigned int base,
                  unsigned long *res);
int mkrn_kstrtol(const char *s, unsigned int base, long *res);
int mkrn_kstrtouint(const char *s, unsigned int base,
                    unsigned int *res);
int mkrn_kstrtoint(const char *s, unsigned int base, int *res);
