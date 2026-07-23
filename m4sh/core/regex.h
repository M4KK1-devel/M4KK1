/*
 * M4KK1 4P1 - regex.h
 * Description: Regular expression engine header
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#pragma once

#include <stdint.h>

#define REGEX_MAX_GROUPS 10
#define REGEX_MAX_NODES 256

typedef struct {
    int start;
    int end;
} regex_match_t;

typedef struct {
    regex_match_t groups[REGEX_MAX_GROUPS];
    int num_groups;
} regex_result_t;

typedef struct regex_node {
    int type;
    int ch;
    struct regex_node *next;
    struct regex_node *child;
    int min_rep;
    int max_rep;
    int greedy;
    int group_id;
    int negated;
    char char_class[256];
} regex_node_t;

typedef struct {
    regex_node_t nodes[REGEX_MAX_NODES];
    int node_count;
    regex_node_t *root;
} regex_t;

int regex_compile(regex_t *re, const char *pattern);
int regex_match(regex_t *re, const char *text, regex_result_t *result);
int regex_search(regex_t *re, const char *text, regex_result_t *result);
void regex_free(regex_t *re);
