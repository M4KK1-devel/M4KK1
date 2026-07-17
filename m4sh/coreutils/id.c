#include "../m4sh.h"

void musr_cmd_id(int ac, char **av)
{
    (void)ac;
    (void)av;
    uint32_t uid = musr_sc_getuid();
    uint32_t euid = musr_sc_geteuid();
    out_puts("uid=");
    print_u32(uid);
    if (uid == 0)
        out_puts("(root)");
    out_puts(" euid=");
    print_u32(euid);
    out_puts("\n");
}
