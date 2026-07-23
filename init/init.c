#include "../m4sh/m4sh.h"

int out_fd = 1;
char cwd[256] = "/";

void _start(void)
{
    for (;;) {
        int ret = m4k_spawn("/bin/login", 0);
        ser_puts("init: spawn /bin/login returned ");
        if (ret < 0)
            ser_puts("error\n");
        else
            ser_puts("ok\n");
        ser_puts("init: halting\n");
        for (;;)
            __asm__("hlt");
    }
}
