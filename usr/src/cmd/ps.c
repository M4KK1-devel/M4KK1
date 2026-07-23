/*
 * M4KK1 4P1 - ps.c
 * Description: ps command - show process list
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 */

#include "m4sh.h"

static const char *state_str(uint32_t st)
{
    switch (st) {
    case 0:
        return "RUN";
    case 1:
        return "RDY";
    case 2:
        return "BLK";
    case 3:
        return "TER";
    default:
        return "???";
    }
}

static void print_state(uint32_t st)
{
    switch (st) {
    case 0:
        c_cyn();
        out_puts("RUN ");
        break;
    case 1:
        c_grn();
        out_puts("RDY ");
        break;
    case 2:
        c_ylw();
        out_puts("BLK ");
        break;
    case 3:
        c_red();
        out_puts("TER ");
        break;
    default:
        out_puts("??? ");
        break;
    }
    c_wht();
}

/**
 * musr_cmd_ps - Display process status
 * @ac: argument count
 * @av: argument vector
 *
 * Return: void
 */
void musr_cmd_ps(int ac, char **av)
{
    int all = 0, lng = 0, usr = 0, full = 0;

    for (int j = 1; j < ac; j++) {
        if (av[j][0] == '-' && av[j][1] != '-') {
            const char *q = av[j] + 1;
            while (*q) {
                if (*q == 'a')
                    all = 1;
                else if (*q == 'l')
                    lng = 1;
                else if (*q == 'u')
                    usr = 1;
                else if (*q == 'f')
                    full = 1;
                q++;
            }
        } else if (musr_strcmp(av[j], "--help") == 0) {
            out_puts("Usage: ps [options]\n");
            out_puts("Report process status.\n\n");
            out_puts("  -a, --all   show all processes\n");
            out_puts("  -l, --long  long format\n");
            out_puts("  -u, --user  user format\n");
            out_puts("  -f, --full  full format\n");
            out_puts("      --help  display this help\n");
            return;
        }
    }

    struct procinfo buf[PROCBUF_MAX];
    int n = musr_sc_getprocs(buf, PROCBUF_MAX);
    if (n < 0) {
        out_puts("ps: error\n");
        return;
    }

    if (lng) {
        c_ylw();
        out_puts("  PID  PPID STATE     NAME\n");
        c_wht();
        for (int k = 0; k < n; k++) {
            print_u32(buf[k].pid);
            out_puts("  ");
            print_u32(buf[k].ppid);
            out_puts("  ");
            print_state(buf[k].state);
            out_puts("  ");
            out_puts(buf[k].name);
            out_putc('\n');
        }
    } else if (usr) {
        c_ylw();
        out_puts("USER    PID  PPID STATE     NAME\n");
        c_wht();
        for (int k = 0; k < n; k++) {
            out_puts("root   ");
            print_u32(buf[k].pid);
            out_puts("  ");
            print_u32(buf[k].ppid);
            out_puts("  ");
            print_state(buf[k].state);
            out_puts("  ");
            out_puts(buf[k].name);
            out_putc('\n');
        }
    } else if (full) {
        c_ylw();
        out_puts("USER    PID  PPID STATE     NAME\n");
        c_wht();
        for (int k = 0; k < n; k++) {
            out_puts("root   ");
            print_u32(buf[k].pid);
            out_puts("  ");
            print_u32(buf[k].ppid);
            out_puts("  ");
            print_state(buf[k].state);
            out_puts("  ");
            out_puts(buf[k].name);
            out_putc('\n');
        }
    } else {
        c_ylw();
        out_puts("  PID STATE  NAME\n");
        c_wht();
        for (int k = 0; k < n; k++) {
            print_u32(buf[k].pid);
            out_puts("  ");
            print_state(buf[k].state);
            out_puts("  ");
            out_puts(buf[k].name);
            out_putc('\n');
        }
    }

    c_grn();
    out_puts("Total: ");
    print_u32(n);
    out_puts(" process(es)\n");
    c_rst();
}
