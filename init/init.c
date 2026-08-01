#include "../m4sh/m4sh.h"

int out_fd = 1;
char cwd[256] = "/";

void _start(void)
{
    ser_puts("\n[INIT] ========================================\n");
    ser_puts("[INIT] Init process started\n");
    ser_puts("[INIT] ========================================\n");

    /* Phase 1: bring up the Copland display server (graphical HAL) */
    ser_puts("[INIT] Starting Copland (display server)...\n");
    int ret = m4k_spawn("/bin/copland", 0);

    if (ret < 0) {
        ser_puts("[INIT] Copland spawn failed, falling back to serial login...\n");
    } else {
        ser_puts("[INIT] Copland returned, falling back to serial login...\n");
    }

    /* Fallback to serial login */
    for (;;) {
        ser_puts("[INIT] Calling m4k_spawn(/bin/login)...\n");
        ret = m4k_spawn("/bin/login", 0);
        if (ret < 0) {
            ser_puts("[INIT] Login spawn failed, retrying...\n");
            for (volatile int i = 0; i < 30000000; i++);
        } else {
            ser_puts("[INIT] Login returned, retrying...\n");
        }
    }
}
