/*
 * M4KK1 4P1 - regex.c
 * Description: Regular expression engine implementation
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "regex.h"
#include "../m4sh.h"

#define NODE_LITERAL 1
#define NODE_DOT 2
#define NODE_CLASS 3
#define NODE_ANCHOR_START 4
#define NODE_ANCHOR_END 5
#define NODE_GROUP 6
#define NODE_ALT 7

static regex_node_t *alloc_node(regex_t *re) {
    if (re->node_count >= REGEX_MAX_NODES)
        return NULL;
    regex_node_t *node = &re->nodes[re->node_count++];
    memset(node, 0, sizeof(regex_node_t));
    node->min_rep = 1;
    node->max_rep = 1;
    node->greedy = 1;
    node->group_id = -1;
    return node;
}

static int parse_char_class(const char **pp, regex_node_t *node) {
    const char *p = *pp;
    if (*p != '[')
        return -1;
    p++;
    
    int negated = 0;
    if (*p == '^') {
        negated = 1;
        p++;
    }
    
    memset(node->char_class, 0, 256);
    
    while (*p && *p != ']') {
        int c = *p++;
        if (*p == '-' && p[1] && p[1] != ']') {
            p++;
            int end = *p++;
            for (int i = c; i <= end; i++)
                node->char_class[i] = 1;
        } else {
            node->char_class[c] = 1;
        }
    }
    
    if (*p == ']')
        p++;
    
    node->negated = negated;
    *pp = p;
    return 0;
}

static regex_node_t *parse_expr(regex_t *re, const char **pp, int *group_id);

static regex_node_t *parse_atom(regex_t *re, const char **pp, int *group_id) {
    const char *p = *pp;
    regex_node_t *node = NULL;
    
    if (*p == '(') {
        p++;
        node = alloc_node(re);
        if (!node)
            return NULL;
        node->type = NODE_GROUP;
        node->group_id = (*group_id)++;
        node->child = parse_expr(re, &p, group_id);
        if (*p == ')')
            p++;
    } else if (*p == '[') {
        node = alloc_node(re);
        if (!node)
            return NULL;
        node->type = NODE_CLASS;
        if (parse_char_class(&p, node) < 0)
            return NULL;
    } else if (*p == '.') {
        node = alloc_node(re);
        if (!node)
            return NULL;
        node->type = NODE_DOT;
        p++;
    } else if (*p == '^') {
        node = alloc_node(re);
        if (!node)
            return NULL;
        node->type = NODE_ANCHOR_START;
        p++;
    } else if (*p == '$') {
        node = alloc_node(re);
        if (!node)
            return NULL;
        node->type = NODE_ANCHOR_END;
        p++;
    } else if (*p == '\\') {
        p++;
        if (*p) {
            node = alloc_node(re);
            if (!node)
                return NULL;
            node->type = NODE_LITERAL;
            node->ch = *p++;
        }
    } else if (*p && *p != '|' && *p != ')') {
        node = alloc_node(re);
        if (!node)
            return NULL;
        node->type = NODE_LITERAL;
        node->ch = *p++;
    }
    
    if (node && *p) {
        if (*p == '*') {
            node->min_rep = 0;
            node->max_rep = -1;
            p++;
        } else if (*p == '+') {
            node->min_rep = 1;
            node->max_rep = -1;
            p++;
        } else if (*p == '?') {
            node->min_rep = 0;
            node->max_rep = 1;
            p++;
        }
        if (*p == '?') {
            node->greedy = 0;
            p++;
        }
    }
    
    *pp = p;
    return node;
}

static regex_node_t *parse_concat(regex_t *re, const char **pp, int *group_id) {
    regex_node_t *first = parse_atom(re, pp, group_id);
    if (!first)
        return NULL;
    
    regex_node_t *last = first;
    while (**pp && **pp != '|' && **pp != ')') {
        regex_node_t *next = parse_atom(re, pp, group_id);
        if (!next)
            break;
        last->next = next;
        last = next;
    }
    
    return first;
}

static regex_node_t *parse_expr(regex_t *re, const char **pp, int *group_id) {
    regex_node_t *first = parse_concat(re, pp, group_id);
    if (!first)
        return NULL;
    
    if (**pp == '|') {
        (*pp)++;
        regex_node_t *alt = alloc_node(re);
        if (!alt)
            return NULL;
        alt->type = NODE_ALT;
        alt->child = first;
        alt->next = parse_expr(re, pp, group_id);
        return alt;
    }
    
    return first;
}

int regex_compile(regex_t *re, const char *pattern) {
    memset(re, 0, sizeof(regex_t));
    const char *p = pattern;
    int group_id = 0;
    re->root = parse_expr(re, &p, &group_id);
    return re->root ? 0 : -1;
}

static int match_node(regex_node_t *node, const char *text, const char **pp, regex_result_t *result);

static int match_class(regex_node_t *node, int c) {
    int in_class = node->char_class[c & 0xFF];
    return node->negated ? !in_class : in_class;
}

static int match_repetition(regex_node_t *node, const char *text, const char **pp, regex_result_t *result) {
    const char *p = *pp;
    int count = 0;
    const char *positions[256];
    
    while (node->max_rep < 0 || count < node->max_rep) {
        const char *save = p;
        regex_node_t temp = *node;
        temp.min_rep = 1;
        temp.max_rep = 1;
        if (!match_node(&temp, text, &p, result))
            break;
        positions[count++] = save;
        if (p == save)
            break;
    }
    
    if (count < node->min_rep)
        return 0;
    
    if (node->greedy) {
        *pp = p;
        return 1;
    } else {
        *pp = positions[node->min_rep > 0 ? node->min_rep - 1 : 0];
        return 1;
    }
}

static int match_node(regex_node_t *node, const char *text, const char **pp, regex_result_t *result) {
    if (!node)
        return 1;
    
    const char *p = *pp;
    
    if (node->type == NODE_ALT) {
        const char *save = p;
        if (match_node(node->child, text, &p, result)) {
            *pp = p;
            return match_node(node->next, text, pp, result);
        }
        p = save;
        if (match_node(node->next, text, &p, result)) {
            *pp = p;
            return 1;
        }
        return 0;
    }
    
    if (node->type == NODE_ANCHOR_START) {
        if (p != text)
            return 0;
        return match_node(node->next, text, pp, result);
    }
    
    if (node->type == NODE_ANCHOR_END) {
        if (*p != '\0')
            return 0;
        return match_node(node->next, text, pp, result);
    }
    
    if (node->min_rep != 1 || node->max_rep != 1) {
        return match_repetition(node, text, pp, result);
    }
    
    const char *start = p;
    int matched = 0;
    
    switch (node->type) {
        case NODE_LITERAL:
            if (*p == node->ch) {
                p++;
                matched = 1;
            }
            break;
        case NODE_DOT:
            if (*p && *p != '\n') {
                p++;
                matched = 1;
            }
            break;
        case NODE_CLASS:
            if (*p && match_class(node, *p)) {
                p++;
                matched = 1;
            }
            break;
        case NODE_GROUP:
            if (node->group_id >= 0 && node->group_id < REGEX_MAX_GROUPS) {
                result->groups[node->group_id].start = (int)(p - text);
            }
            if (match_node(node->child, text, &p, result)) {
                if (node->group_id >= 0 && node->group_id < REGEX_MAX_GROUPS) {
                    result->groups[node->group_id].end = (int)(p - text);
                }
                matched = 1;
            }
            break;
    }
    
    if (matched) {
        *pp = p;
        return match_node(node->next, text, pp, result);
    }
    
    return 0;
}

int regex_match(regex_t *re, const char *text, regex_result_t *result) {
    memset(result, 0, sizeof(regex_result_t));
    const char *p = text;
    return match_node(re->root, text, &p, result);
}

int regex_search(regex_t *re, const char *text, regex_result_t *result) {
    memset(result, 0, sizeof(regex_result_t));
    
    for (const char *p = text; ; p++) {
        const char *start = p;
        if (match_node(re->root, text, &start, result)) {
            result->groups[0].start = (int)(p - text);
            result->groups[0].end = (int)(start - text);
            return 1;
        }
        if (!*p)
            break;
    }
    
    return 0;
}

void regex_free(regex_t *re) {
    (void)re;
}
