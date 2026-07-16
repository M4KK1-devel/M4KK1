#include <signal.h>
#include <process.h>
#include <kernel.h>
#include <console.h>
#include <string.h>

static const char *sig_name(int sig)
{
    switch (sig) {
    case M4K_SIGABRT: return "SIGABRT";
    case M4K_SIGKILL: return "SIGKILL";
    case M4K_SIGTERM: return "SIGTERM";
    case M4K_SIGSTOP: return "SIGSTOP";
    case M4K_SIGCONT: return "SIGCONT";
    case M4K_SIGTRAP: return "SIGTRAP";
    case M4K_SIGCHLD: return "SIGCHLD";
    case M4K_SIGPIPE: return "SIGPIPE";
    case M4K_SIGUSR1: return "SIGUSR1";
    case M4K_SIGUSR2: return "SIGUSR2";
    default: return "UNKNOWN";
    }
}

int mkrn_signal_is_fatal(int sig)
{
    switch (sig) {
    case M4K_SIGKILL:
    case M4K_SIGABRT:
        return 1;
    default:
        return 0;
    }
}

int mkrn_signal_is_stop(int sig)
{
    return sig == M4K_SIGSTOP;
}

int mkrn_signal_is_cont(int sig)
{
    return sig == M4K_SIGCONT;
}

int mkrn_signal_can_mask(int sig)
{
    switch (sig) {
    case M4K_SIGKILL:
    case M4K_SIGSTOP:
    case M4K_SIGCONT:
    case M4K_SIGTRAP:
        return 0;
    default:
        return 1;
    }
}

int mkrn_signal_default_action(int sig)
{
    switch (sig) {
    case M4K_SIGABRT:
    case M4K_SIGKILL:
    case M4K_SIGPIPE:
        return 1; /* terminate */
    case M4K_SIGSTOP:
        return 2; /* stop */
    case M4K_SIGCONT:
        return 3; /* continue */
    case M4K_SIGCHLD:
    case M4K_SIGUSR1:
    case M4K_SIGUSR2:
    default:
        return 0; /* ignore */
    case M4K_SIGTERM:
        return 1; /* terminate */
    }
}

void mkrn_signal_deliver(mkrn_process_t *proc, int sig)
{
    if (!proc || sig <= 0 || sig >= M4K_NSIG) {
        M4K_LOG_WARN("signal_deliver: invalid signal or target");
        return;
    }

    if (proc->pid == 1 && sig == M4K_SIGKILL) {
        M4K_LOG_INFO("SIGKILL to PID 1 ignored");
        return;
    }

    proc->pending_signals |= (1 << sig);

    if (mkrn_signal_is_fatal(sig)) {
        proc->exit_status = sig;
        mkrn_process_exit(sig);
    } else if (mkrn_signal_is_stop(sig)) {
        proc->state_tags |= M4K_STOPPED;
        proc->state_tags &= ~M4K_SCHED_READY;
        M4K_LOG_INFO("signal_deliver: stopped ");
        mkrn_console_write(proc->name);
        mkrn_console_write("\n");
    } else if (mkrn_signal_is_cont(sig)) {
        proc->state_tags &= ~M4K_STOPPED;
        if (!(proc->state_tags & M4K_STATE_SCHED_MASK)) {
            proc->state_tags |= M4K_SCHED_READY;
        }
    }
}

void mkrn_signal_init(void)
{
    M4K_LOG_INFO("Signal subsystem initialized");
}
