#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <console.h>
#include <kernel.h>
#include <sessions.h>
#include <process.h>

uint32_t m4k_syscall_register_session_impl(uint32_t tty_ptr, uint32_t pid,
                                           uint32_t username_ptr,
                                           uint32_t arg4, uint32_t arg5)
{
    (void)pid;
    (void)arg4;
    (void)arg5;

    /* Copy user-space strings to kernel buffers before any use */
    char tty_buf[M4K_SESSION_TTY_LEN];
    char user_buf[M4K_SESSION_USERNAME_LEN];
    tty_buf[0] = '\0';
    user_buf[0] = '\0';

    const char *tty_src = (const char *)tty_ptr;
    const char *user_src = (const char *)username_ptr;
    if (tty_src)
        mkrn_strncpy(tty_buf, tty_src, M4K_SESSION_TTY_LEN - 1);
    if (user_src)
        mkrn_strncpy(user_buf, user_src, M4K_SESSION_USERNAME_LEN - 1);
    tty_buf[M4K_SESSION_TTY_LEN - 1] = '\0';
    user_buf[M4K_SESSION_USERNAME_LEN - 1] = '\0';

    if (tty_buf[0] == '\0' || user_buf[0] == '\0')
        return (uint32_t)-M4K_EINVAL;

    uint32_t uid = mkrn_process_get_uid();
    int ret = mkrn_session_create(tty_buf, uid, user_buf);
    if (ret < 0)
        return (uint32_t)-M4K_EBUSY;

    M4K_LOG_INFO("session: registered session on ");
    M4K_LOG_INFO(tty_buf);
    M4K_LOG_INFO(" for user ");
    M4K_LOG_INFO(user_buf);
    M4K_LOG_INFO("\n");
    return 0;
}

uint32_t m4k_syscall_get_session_list_impl(uint32_t buf_ptr, uint32_t max,
                                           uint32_t arg3, uint32_t arg4, uint32_t arg5)
{
    (void)arg3; (void)arg4; (void)arg5;
    struct m4k_session *buf = (struct m4k_session *)buf_ptr;
    if (!buf || max == 0)
        return (uint32_t)-M4K_EINVAL;

    return (uint32_t)mkrn_sessions_get_all(buf, (int)max);
}
