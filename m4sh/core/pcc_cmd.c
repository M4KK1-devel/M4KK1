/*
 * M4KK1 4P1 - pcc_cmd.c
 * Description: m4sh built-in front end for the self-hosted PCC compiler.
 *
 * Copyright (c) 2026 Yaku Makki
 * SPDX-License-Identifier: 4P1-Custom
 *
 * The kernel's m4k_spawn() carries no argv, so the shell hands the
 * command line to /bin/pcc through /tmp/pcc.cmd (see collect_args() in
 * usr/src/tools/pcc/pcc.c).  The shell forks, the child execs pcc in
 * its own 0xB00000 zone and the parent waits, so the running shell
 * image at 0x800000 stays intact.
 */

#include "../m4sh.h"

#define PCC_CMD_FILE "/tmp/pcc.cmd"
#define PCC_BIN      "/bin/pcc"

static int pcc_append(char *line, int pos, int max, const char *s)
{
    while (*s && pos < max - 1)
        line[pos++] = *s++;
    return pos;
}

static int pcc_run(int ac, char **av)
{
    const char *prog = (ac > 0 && av[0] && av[0][0]) ? av[0] : "pcc";

    if (ac < 2) {
        out_puts("usage: ");
        out_puts(prog);
        out_puts(" [-v] [-o output] file.c\n");
        return 1;
    }

    musr_sc_mkdir("/tmp");

    char line[512];
    int pos = 0;
    pos = pcc_append(line, pos, (int)sizeof(line), prog);

    for (int i = 1; i < ac; i++) {
        if (pos >= (int)sizeof(line) - 2)
            break;
        line[pos++] = ' ';
        if (av[i][0] != '-') {
            char abs[256];
            cwd_to_abs(av[i], abs, sizeof(abs));
            pos = pcc_append(line, pos, (int)sizeof(line), abs);
        } else {
            pos = pcc_append(line, pos, (int)sizeof(line), av[i]);
            if (musr_strcmp(av[i], "-o") == 0 && i + 1 < ac) {
                char abs[256];
                i++;
                if (pos < (int)sizeof(line) - 2)
                    line[pos++] = ' ';
                cwd_to_abs(av[i], abs, sizeof(abs));
                pos = pcc_append(line, pos, (int)sizeof(line), abs);
            }
        }
    }
    line[pos] = '\0';

    int fd = musr_sc_open(PCC_CMD_FILE, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        c_red();
        out_puts("m4sh: cannot write " PCC_CMD_FILE "\n");
        c_rst();
        return 1;
    }
    musr_sc_write(fd, line, pos);
    musr_sc_close(fd);

    int pid = musr_sc_fork();
    if (pid < 0) {
        c_red();
        out_puts("m4sh: fork failed\n");
        c_rst();
        return 1;
    }
    if (pid == 0) {
        m4k_spawn(PCC_BIN, 0);
        ser_puts("m4sh: spawn " PCC_BIN " failed\n");
        m4k_exit(127);
    }

    int status = 0;
    musr_sc_waitpid(pid, &status, 0);
    return status;
}

void musr_cmd_pcc(int ac, char **av)
{
    pcc_run(ac, av);
}

void musr_cmd_cc(int ac, char **av)
{
    pcc_run(ac, av);
}
