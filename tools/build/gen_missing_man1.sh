#!/bin/bash
# gen_missing_man1.sh — write the missing man1 pages (zh_CN)
set -eu
cd "$(dirname "$0")/../../"
D=docs/zh_CN/man/man1
mkdir -p "$D"
D_="2026-09-05"

w() { # $1=name $2=title-zh $3=oneline-desc $4=body (heredoc via stdin)
    local f="$D/$1.1"
    [ -f "$f" ] && { echo "SKIP $1 (exists)"; return; }
    { echo ".\\\" M4KK1 4P1 - $1.1"
      echo ".TH ${1^^} 1 \"$D_\" \"M4KK1 1.0\" \"M4KK1 用户命令\""
      echo ".SH 名称"
      echo "$1 \\- $2"
      echo ".SH 概要"
      echo ".B $1"
      echo "[\\fI参数\\fR]"
      echo ".SH 描述"
      cat
      echo ".SH 参见"
      echo ".BR m4sh (1),"
      echo ".BR man (1)"
    } > "$f"
    echo "WROTE $1"
}

w cd "切换工作目录 (Zsh 风格)" <<'EOF'
.B cd
[\fI目录\fR]
.TP
切换到指定目录。无参数时回到用户主目录。
.B cd \-
回到上一个工作目录（Zsh 风格）。
EOF

w pwd "打印当前工作目录" <<'EOF'
打印当前工作目录的绝对路径。等价于 readlink(\"/proc/self/cwd\") 语义，经
.I SYS_GETCWD
实现。
EOF

w clear "清空终端屏幕" <<'EOF'
向终端输出 ANSI 清屏转义序列并复位光标到左上角。
EOF

w help "列出可用命令" <<'EOF'
列出当前 shell 内所有已注册命令的名称。
EOF

w export "设置环境变量" <<'EOF'
.B export \fIVAR\fR=\fIvalue\fR
.TP
设置环境变量。最多 16 个变量，每个最大 128 字节。展开语法：\fB$VAR\fR 与 \fB${\fIVAR\fB}\fR。
EOF

w exit "退出 shell" <<'EOF'
结束当前 shell 会话。在 m4sh 中等价于 Ctrl+D（空行时）。
EOF

w cp "复制文件" <<'EOF'
.B cp \fI源\fR \fI目标\fR
.TP
复制文件内容到目标路径。目标为目录时复制到目录内同名文件。
EOF

w mv "移动或重命名文件" <<'EOF'
.B mv \fI源\fR \fI目标\fR
.TP
移动文件或目录到目标路径；同目录下相当于重命名（经
.I SYS_RENAME
实现）。
EOF

w rm "删除文件" <<'EOF'
.B rm \fI文件\fR
.TP
删除指定文件（经
.I SYS_UNLINK
实现）。不能删除目录（见
.BR rmdir (1)）。
EOF

w mkdir "创建目录" <<'EOF'
.B mkdir \fI目录\fR
.TP
创建一个新目录。
EOF

w rmdir "删除空目录" <<'EOF'
.B rmdir \fI目录\fR
.TP
删除指定目录，目录必须为空。
EOF

w touch "创建空文件或更新时间戳" <<'EOF'
.B touch \fI文件\fR
.TP
文件不存在时创建空文件（O_CREAT）；存在时更新修改时间戳。
EOF

w dd "复制块设备/文件数据" <<'EOF'
.B dd if=\fI输入\fR of=\fI输出\fR [bs=\fIN\fR] [count=\fIN\fR]
.TP
按块复制数据。默认 bs=512。count 省略时复制到输入末尾。
EOF

w df "显示文件系统磁盘使用" <<'EOF'
显示已挂载文件系统的总空间、已用空间与可用空间（经
.I SYS_STATFS
实现）。
EOF

w diff "比较两个文件" <<'EOF'
.B diff \fI文件1\fR \fI文件2\fR
.TP
逐行比较两个文件，输出差异行。用于快速校验文件内容一致性。
EOF

w free "显示内存使用统计" <<'EOF'
以 KB 为单位显示系统总内存、已用与空闲（数据来自
.IR sysinfo ）。
EOF

w kill "向进程发送信号" <<'EOF'
.B kill [\-signal] \fIpid\fR
.TP
向指定 PID 发送信号（默认 SIGKILL 对应数值）。信号可为负数形式 \-N 指定。
EOF

w nice "设置进程优先级（未实现）" <<'EOF'
当前版本为占位实现，执行时输出 "nice: not implemented"。
EOF

w time "测量命令执行时间" <<'EOF'
.B time \fI命令\fR [\fI参数...\fR]
.TP
执行命令并报告 real（墙钟）时间，单位毫秒。
EOF

w at "在指定时间执行命令（调度队列）" <<'EOF'
.B at \fI时间\fR \fI命令\fR [\fI参数...\fR]
.TP
注册一次性计划任务。时间到后由调度器自动执行。包含危险字符的任务会被拒绝。
EOF

w batch "在系统空闲时执行命令" <<'EOF'
.B batch \fI命令\fR [\fI参数\fR]
.TP
注册空闲时段执行的计划任务。
EOF

w beep "播放提示音" <<'EOF'
.B beep [\fI频率\fR \fI时长\fR]
.TP
通过 SB16 驱动播放方波提示音（经
.I M4K_SYS_BEEP
系统调用）。无参数时播放默认音。
EOF

w blkid "显示块设备信息" <<'EOF'
列出系统块设备及其容量、类型等信息，含 USAGE= 属性。
EOF

w calc "命令行计算器" <<'EOF'
.B calc [\fI表达式\fR]
.TP
四则运算（支持括号）。无参数时进入交互模式（help 查看子命令，exit 退出）。
EOF

w ifconfig "显示网络接口配置" <<'EOF'
显示网络接口（e1000）的 IP 地址与配置状态。
EOF

w ping "测试网络连通性" <<'EOF'
.B ping \fIip\fR
.TP
向目标 IP 发送 ICMP 回显请求并报告往返时间。
EOF

w mount "挂载文件系统" <<'EOF'
挂载文件系统到指定挂载点。
EOF

w umount "卸载文件系统" <<'EOF'
.B umount \fI目标\fR
.TP
卸载已挂载的文件系统。
EOF

w uname "打印系统信息" <<'EOF'
显示操作系统名称。支持选项：\-a 全部信息（内核名/版本/构建日期）。
EOF

w last "显示系统开机时长" <<'EOF'
显示系统已运行时间（uptime）。
EOF

w userlog "显示登录历史" <<'EOF'
显示历史登录记录（用户、终端、时间）。
EOF

w wget "HTTP 下载工具" <<'EOF'
.B wget http://\fIip\fR[:\fIport\fR]/\fI路径\fR [\fI输出文件\fR]
.TP
经 HTTP GET 下载远程文件。输出文件省略时使用 URL 末段作为文件名。
EOF

w sead "流编辑器（sed/awk 替代）" <<'EOF'
.B sead [\fI选项\fR] '\fI脚本\fR' [\fI文件...\fR]
.TP
按脚本逐行处理输入。选项：
.B \-e 'script'
执行脚本、
.B \-f file
从文件读脚本、
.B \-n
抑制自动打印、
.B \-i[SUFFIX]
就地编辑（可选备份）、
.B \-\-csv
CSV 模式、
.B \-\-debug
调试输出。
EOF

w spawn "派生新进程" <<'EOF'
.B spawn /bin/\fI程序\fR
.TP
经
.I M4K_SYS_SPAWN
系统调用派生新进程运行指定 ELF。
EOF

w login "登录工具" <<'EOF'
.B login [\fI用户名\fR]
.TP
提示输入密码并校验（passwd.db 加盐 SHA-256）。成功后启动用户会话。
EOF

w cc "PCC 编译器前端" <<'EOF'
.B cc [\fI文件.c\fR]
.TP
PCC 可移植 C 编译器的别名（usr/src/tools/pcc/pcc.elf 安装为 /bin/cc）。用法详见
.BR pcc (1)。
EOF

w pcc "PCC 可移植 C 编译器" <<'EOF'
.B pcc [\fI选项\fR] \fI文件.c\fR
.TP
M4KK1 自托管 C 编译器（20220331 版）。选项：
.B \-v
打印版本、
.B \-S
输出汇编、
.B \-c
只编译不链接。
EOF

w fm "图形文件管理器" <<'EOF'
.B spawn /bin/fm
.TP
Copland 图形文件管理器，支持多标签页。像素缓冲经
.I m4k_gfx_blit
上屏，键盘事件驱动操作。
EOF

w terminal "图形终端模拟器" <<'EOF'
.B spawn /bin/terminal
.TP
Copland 图形终端，内部托管真实 m4sh 子进程。
EOF

w logview "系统日志查看器" <<'EOF'
.B spawn /bin/logview
.TP
Copland 客户端，读取 /var/log/messages 到滚动视图。
EOF

w clock "桌面时钟" <<'EOF'
.B spawn /bin/clock
.TP
Copland 客户端，显示 RTC 时间与日期。
EOF

w info "系统信息面板" <<'EOF'
.B spawn /bin/info
.TP
Copland 客户端，显示内存（sysinfo）、运行时长等系统信息。
.EOF

w calcg "图形计算器" <<'EOF'
.B spawn /bin/calcg
.TP
Copland 图形计算器：四则运算、支持括号与连续计算。
EOF

w cal "显示日历" <<'EOF'
.B cal [[\fI月\fR] \fI年\fR]
.TP
显示指定月份的日历网格。无参数时显示当月。
EOF

w sprach "Sprach 窗口管理器" <<'EOF'
.B spawn /bin/sprach
.TP
Sprach 窗口管理器（详见
.BR desktop (7)）：桌面图标、任务栏、窗口堆叠与合成。核心（mode-independent）+ 多模块（stack/scroll/tiling）。
EOF

w copland "Copland 显示服务器" <<'EOF'
.B copland
.TP
Copland 显示服务器：窗口 surface 管理、共享内存合成、输入分发。Sprach/MDM 及所有桌面应用均为其客户端。
EOF

w mdm "MDM 显示管理器（图形登录）" <<'EOF'
.B mdm
.TP
MDM 显示管理器：图形登录界面、会话选择、多会话切换（m4k_register_session / get_session_list）。
EOF

w automission "列出自动任务" <<'EOF'
列出已注册的 automission 计划任务（名称、调度、描述）。
EOF

w fsck "恢复模式文件系统检查器" <<'EOF'
恢复模式（recovery ISO）下的用户态文件系统检查器。修复 YAFS 结构不一致。
EOF

w reset-passwd "恢复模式密码重置工具" <<'EOF'
恢复模式（recovery ISO）下的密码重置工具。用于 root 密码丢失时恢复访问。
EOF

w gfx_test "图形栈测试程序" <<'EOF'
图形栈自检：帧缓冲信息、flip、blit、渐变填充等断言。用于回归测试。
EOF

w cptest "Copland 表面测试程序" <<'EOF'
Copland surface 生命周期测试：创建 surface、收发键盘事件、断言后销毁 surface 退出。用于回归测试。
EOF

echo "DONE man1"
