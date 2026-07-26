#include "../m4sh/m4sh.h"

int out_fd = 1;
char cwd[256] = "/";

void _start(void)
{
    ser_puts("\n[INIT] ========================================\n");
    ser_puts("[INIT] Init process started\n");
    ser_puts("[INIT] ========================================\n");

    /* Launch flip_test for VESA display verification */
    ser_puts("[INIT] Launching flip_test (VESA display verification)...\n");
    int ret = m4k_spawn("/bin/flip_test", 0);

    if (ret < 0) {
        ser_puts("[INIT] flip_test failed (ret=");
        print_u32((uint32_t)ret);
        ser_puts("), trying mdm_mini...\n");
        ret = m4k_spawn("/bin/mdm_mini", 0);
    }

    if (ret < 0) {
        ser_puts("[INIT] mdm_mini failed, trying full MDM...\n");
        ret = m4k_spawn("/bin/mdm", 0);
    }

    if (ret < 0) {
        ser_puts("[INIT] All graphical programs failed, falling back to serial login.\n");
    } else {
        ser_puts("[INIT] Graphical program returned, falling back to serial login.\n");
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
