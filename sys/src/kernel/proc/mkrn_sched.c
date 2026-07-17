#include <stdint.h>
#include <stdbool.h>
#include <m4k/state.h>
#include <process.h>

struct mkrn_process *mkrn_pick_next_task(void)
{
    mkrn_process_t *current = mkrn_process_get_current();
    mkrn_process_t *p = NULL;
    uint32_t best_priority = 999;

    for (uint32_t i = 0; ; i++) {
        struct mkrn_procinfo buf[64];
        int count = mkrn_process_fill_info(buf, 64);
        if (count <= 0) break;

        for (int j = 0; j < count; j++) {
            p = mkrn_process_find((pid_t)buf[j].pid);
            if (!p) continue;
            if (p == current) continue;
            if ((p->state_tags & M4K_SCHED_READY) &&
                !(p->state_tags & M4K_STOPPED) &&
                p->priority < best_priority) {
                best_priority = p->priority;
            }
        }
        break;
    }

    return NULL;
}
