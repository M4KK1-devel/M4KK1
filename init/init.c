#include "../m4sh/m4sh.h"

int out_fd = 1;
char cwd[256] = "/";

void _start(void)
{
    ser_puts("\n[INIT] ========================================\n");
    ser_puts("[INIT] Init process started\n");
    ser_puts("[INIT] ========================================\n");

#ifdef M4K_CMD_ONLY
    /* cmd-only / recovery: no display server, straight to serial login */
    ser_puts("[INIT] Command-line mode: skipping display server\n");
#ifdef M4K_RECOVERY
    /* Run fsck in place (spawn, no fork - fork stack copy is
     * unreliable); fsck hands over to /bin/login when done. */
    ser_puts("[INIT] Recovery mode: running /bin/fsck...\n");
    int fsck_ret = m4k_spawn("/bin/fsck", 0);
    ser_puts("[INIT] fsck failed to start (ret=");
    ser_puts((fsck_ret < 0) ? "err" : "0");
    ser_puts("), falling back to login\n");
#endif
#else
    /* Full desktop: init execs the graphical display manager (MDM).
     * On successful login MDM forks Copland (which spawns Sprach) and
     * exits back here; we respawn MDM to get a graphical login-screen
     * lock the next time the desktop session ends. */
    for (;;) {
        ser_puts("[INIT] Starting MDM (graphical login)...\n");
        int ret = m4k_spawn("/bin/mdm", 0);
        if (ret < 0) {
            ser_puts("[INIT] MDM spawn failed, retrying...\n");
            for (volatile int i = 0; i < 30000000; i++);
        } else {
            ser_puts("[INIT] MDM exited, respawning...\n");
            for (volatile int i = 0; i < 10000000; i++);
        }
    }
#endif

    /* Fallback to serial login (cmd-only builds reach this) */
    for (;;) {
        ser_puts("[INIT] Calling m4k_spawn(/bin/login)...\n");
        int ret = m4k_spawn("/bin/login", 0);
        if (ret < 0) {
            ser_puts("[INIT] Login spawn failed, retrying...\n");
            for (volatile int i = 0; i < 30000000; i++);
        } else {
            ser_puts("[INIT] Login returned, retrying...\n");
        }
    }
}
