#!/bin/bash
# gen_missing_man2.sh — write the missing man2 syscall pages (zh_CN)
# Sources of truth: m4sh/m4sh.h inline wrappers + sys/src/kernel/syscall_m4k.c
# registered handlers. Only registered syscalls get pages.
set -eu
cd "$(dirname "$0")/../../"
D=docs/zh_CN/man/man2
mkdir -p "$D"
D_="2026-09-05"

w() { # $1=name $2=title $3=synopsis-block $4=desc $5=retval $6=errors(may be empty) $7=seealso
    local f="$D/$1.2"
    [ -f "$f" ] && { echo "SKIP $1"; return; }
    { echo ".\\\" M4KK1 4P1 - $1.2"
      echo ".TH ${1^^} 2 \"$D_\" \"M4KK1 1.0\" \"M4KK1 系统调用\""
      echo ".SH 名称"
      echo "$1 \\- $2"
      echo ".SH 概要"
      echo ".nf"
      echo "#include <m4k/syscall.h>"
      echo "$3"
      echo ".fi"
      echo ".SH 描述"
      echo "$4"
      echo ".SH 返回值"
      echo "$5"
      if [ -n "$6" ]; then
        echo ".SH 错误"
        echo "$6"
      fi
      echo ".SH 参见"
      echo "$7"
    } > "$f"
    echo "WROTE $1"
}

w m4k_exit "终止当前进程" \
  'int m4k_exit(int status);' \
  '终止当前进程，status 作为退出码传递给父进程（waitpid 可读取）。' \
  '不返回。' \
  '' \
  '.BR m4k_spawn (2), .BR m4k_waitpid (2)'

w m4k_spawn "派生新进程" \
  'int m4k_spawn(const char *path, uint32_t flags);' \
  '从 path 加载 ELF 并派生为新进程。与 fork 不同，spawn 直接以新程序映像启动。' \
  '成功返回子进程 PID，失败返回负的 M4K_E* 错误码。' \
  '.TP
.B M4K_ENOENT
路径不存在或非 ELF' \
  '.BR m4k_exit (2), .BR m4k_waitpid (2)'

w m4k_waitpid "等待子进程结束" \
  'int m4k_waitpid(int pid, int *status, int options);' \
  '阻塞等待指定子进程（pid=-1 表示任意子进程）退出，退出码写入 status。' \
  '成功返回被回收子进程的 PID；无子进程返回负的 M4K_ECHILD。' \
  '' \
  '.BR m4k_spawn (2), .BR m4k_exit (2)'

w m4k_getpid "获取进程 ID" \
  'int m4k_getpid(void);' \
  '返回调用者进程的 PID。' \
  'PID（恒成功）。' '' ''

w m4k_getppid "获取父进程 ID" \
  'int m4k_getppid(void);' \
  '返回调用者进程父进程的 PID。' \
  '父 PID（恒成功）。' '' ''

w m4k_kill "向进程发送信号" \
  'int m4k_kill(int pid, int sig);' \
  '向进程 pid 发送信号 sig。sig=0 时只做权限检查不发送。' \
  '成功返回 0，失败返回负的 M4K_E* 错误码。' \
  '.TP
.B M4K_ESRCH
进程不存在
.TP
.B M4K_EPERM
无权限向目标进程发送信号' \
  '.BR m4k_getpid (2)'

w m4k_fork_status "查询 fork 状态" \
  'int m4k_fork_status(void);' \
  '配合经典 fork 路径使用：子进程中返回标记，用于区分 fork 前后的执行流。' \
  '状态标记值。' '' ''

w m4k_setns "切换命名空间" \
  'int m4k_setns(const char *path, const char *type, uint32_t flags);' \
  '将调用进程加入 path 指定的命名空间（type 指定命名空间类别）。' \
  '成功返回 0，失败返回负的 M4K_E* 错误码。' \
  '.TP
.B M4K_EPERM
需要特权' \
  ''

w m4k_getprocs "获取进程表快照" \
  'int m4k_getprocs(struct m4k_procinfo *buf, int max);' \
  '将进程表快照写入 buf（最多 max 条）。ps 等工具的数据来源。' \
  '成功返回写入的进程条数，失败返回负错误码。' '' \
  '.BR ps (1)'

w m4k_getuid "获取实际用户 ID" \
  'int m4k_getuid(void);' \
  '返回调用者的实际 UID。' \
  'UID（恒成功）。' '' \
  '.BR m4k_geteuid (2), .BR m4k_setuid (2)'

w m4k_geteuid "获取有效用户 ID" \
  'int m4k_geteuid(void);' \
  '返回调用者的有效 UID。' \
  'EUID（恒成功）。' '' \
  '.BR m4k_getuid (2)'

w m4k_getgid "获取实际组 ID" \
  'int m4k_getgid(void);' \
  '返回调用者的实际 GID。' \
  'GID（恒成功）。' '' \
  '.BR m4k_getegid (2), .BR m4k_setgid (2)'

w m4k_getegid "获取有效组 ID" \
  'int m4k_getegid(void);' \
  '返回调用者的有效 GID。' \
  'EGID（恒成功）。' '' \
  '.BR m4k_getgid (2)'

w m4k_getgroups "获取补充组列表" \
  'int m4k_getgroups(int size, uint32_t *list);' \
  '将调用者的补充组列表写入 list。size=0 时返回组数，用于探测所需缓冲区大小。' \
  '成功返回组数；size 不足时返回负的 M4K_EINVAL。' '' \
  '.BR m4k_setgroups (2), .BR id (1)'

w m4k_setgroups "设置补充组列表" \
  'int m4k_setgroups(int size, const uint32_t *list);' \
  '设置调用者的补充组列表。需要 root 特权。' \
  '成功返回 0，失败返回负的 M4K_EPERM。' \
  '.TP
.B M4K_EPERM
非 root 调用' \
  '.BR m4k_getgroups (2)'

w m4k_chmod "修改文件权限位" \
  'int m4k_chmod(const char *path, int mode);' \
  '修改 path 的权限位。需要文件属主或 root。' \
  '成功返回 0，失败返回负错误码。' \
  '.TP
.B M4K_EPERM
非属主且非 root' \
  ''

w m4k_chown "修改文件属主" \
  'int m4k_chown(const char *path, uint32_t uid, uint32_t gid);' \
  '修改 path 的属主 UID 与属组 GID。仅 root 可用。' \
  '成功返回 0，失败返回负的 M4K_EPERM。' \
  '.TP
.B M4K_EPERM
非 root 调用' \
  '.BR m4k_chmod (2)'

w m4k_access "检查文件访问权限" \
  'int m4k_access(const char *path, int mode);' \
  '以实际 UID/GID 检查对 path 的 mode 权限（R_OK/W_OK/X_OK 组合）。' \
  '允许返回 0，不允许返回负错误码。' '' \
  ''

w m4k_setrlimit "设置资源限制" \
  'int m4k_setrlimit(int resource, const struct m4k_rlimit *lim);' \
  '设置资源上限。resource 取 M4K_RLIMIT_CPU/DATA/STACK/NPROC/NOFILE/MEMLOCK。需特权（上限不可超软限制的提升规则）。' \
  '成功返回 0，失败返回负错误码。' \
  '.TP
.B M4K_EPERM
无特权提升硬限制' \
  '.BR m4k_getrlimit (2)'

w m4k_getrlimit "查询资源限制" \
  'int m4k_getrlimit(int resource, struct m4k_rlimit *lim);' \
  '查询 resource 的软/硬限制，写入 lim（rlim_cur/rlim_max）。' \
  '成功返回 0，失败返回负的 M4K_EINVAL。' '' \
  '.BR m4k_setrlimit (2)'

w m4k_brk "扩展数据段" \
  'long m4k_brk(unsigned long addr);' \
  '移动进程 program break。addr=0 时返回当前 break，用于探测。' \
  '成功返回新 break 地址，失败返回负错误码。' '' \
  '.BR m4k_mmap (2)'

w m4k_yield "主动让出 CPU" \
  'int m4k_yield(void);' \
  '协作式调度下主动让出 CPU 给同优先级就绪进程。图形主循环与轮询循环中用于避免饿死其他任务。' \
  '成功返回 0。' '' \
  ''

w m4k_get_framebuffer_info "获取帧缓冲信息" \
  'int m4k_get_framebuffer_info(struct m4k_framebuffer_info *fb);' \
  '查询 VESA 帧缓冲的物理地址、宽、高、bpp 与 pitch，写入 fb。' \
  '成功返回 0，失败返回负错误码。' '' \
  '.BR m4k_flip (2), .BR desktop (7)'

w m4k_draw_test_pattern "绘制测试图案" \
  'int m4k_draw_test_pattern(void);' \
  '在帧缓冲上绘制内核测试图案（色条），用于显示通路自检。' \
  '成功返回 0。' '' \
  '.BR m4k_flip (2)'

w m4k_get_mouse_event "读取鼠标事件" \
  'int m4k_get_mouse_event(struct m4k_mouse_event *ev);' \
  '非阻塞读取一个鼠标增量事件（dx/dy/buttons/dz 滚轮）。无事件时返回负值。' \
  '有事件返回 0 并填充 ev，无事件返回负值。' '' \
  '.BR m4k_get_mouse_pos (2)'

w m4k_flip "全屏翻页" \
  'int m4k_flip(void);' \
  '将后备帧缓冲整页提交上屏。Copland 合成完成一帧后调用。' \
  '成功返回 0。' '' \
  '.BR m4k_flip_rect (2), .BR m4k_gfx_blit (2)'

w m4k_flip_rect "局部翻页" \
  'int m4k_flip_rect(int x, int y, int w, int h);' \
  '只提交屏幕上的脏矩形区域，减少全屏 flip 的带宽开销。' \
  '成功返回 0。' '' \
  '.BR m4k_flip (2)'

w m4k_update_cursor "重绘硬件光标" \
  'int m4k_update_cursor(void);' \
  '请求内核重绘鼠标光标图层（移动或改形后调用）。' \
  '成功返回 0。' '' \
  '.BR m4k_get_mouse_pos (2)'

w m4k_get_keyboard_event "读取键盘事件" \
  'int m4k_get_keyboard_event(struct m4k_keyboard_event *ev);' \
  '读取一个键盘事件（ascii_char/keycode/modifiers）。无事件时返回负值；调用本身协作式让出 CPU。' \
  '有事件返回 0 并填充 ev，无事件返回负值。' '' \
  '.BR desktop (7)'

w m4k_gfx_blit "位块传送上屏" \
  'int m4k_gfx_blit(int x, int y, int w, int h, const void *src);' \
  '将用户态像素缓冲 src（RGBA32）直接 blit 到屏幕区域 (x,y,w,h)，绕过后备缓冲整页合成。MDM 全屏图标等使用。' \
  '成功返回 0。' '' \
  '.BR m4k_flip (2)'

w m4k_fill_gradient "垂直渐变填充" \
  'int m4k_fill_gradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);' \
  '在裁剪矩形内填充 top→bottom 垂直渐变（颜色按全屏高度插值）。一次系统调用替代逐扫描线 draw_rect 循环。' \
  '成功返回 0。' '' \
  '.BR m4k_draw_test_pattern (2)'

w m4k_beep "播放方波提示音" \
  'int m4k_beep(uint32_t hz, uint32_t ms);' \
  '经 SB16 驱动播放 hz 频率、ms 时长的方波音。' \
  '成功返回 0。' '' \
  '.BR m4k_play_pcm (2), .BR beep (1)'

w m4k_sleep "毫秒级睡眠" \
  'int m4k_sleep(uint32_t ms);' \
  '让当前进程睡眠 ms 毫秒（调度器移出就绪队列，到时唤醒）。' \
  '成功返回 0。' '' \
  '.BR m4k_yield (2)'

w m4k_get_mouse_pos "查询鼠标位置" \
  'int m4k_get_mouse_pos(int32_t *x, int32_t *y);' \
  '查询当前鼠标指针的屏幕坐标。' \
  '成功返回 0 并填充 x/y。' '' \
  '.BR m4k_get_mouse_event (2)'

echo "DONE man2"
