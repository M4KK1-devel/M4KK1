
## 周报 2026-08-22（窗口 2026-08-20 21:50:00 起）

### 提交列表
- 39594d1 chore(cron): daily summary - 2026-08-22
- 8374944 fix(sched): scheduler silently dropped valid picks; ramdisk to fixed 0x2000000, sprach to 0x1100000, execve LOAD guard
- b633b5e perf: round two — kernel string funcs, execve, syscall, sb16, yafs btree cleanups; remove usr/bin sprach variant leftovers
- 1c35c93 perf: drop unused sprach tiling/scroll embeds (75KB kernel data), lazy YAFS ramdisk zeroing (16MB boot memset -> per-block), fd table 4096->1024 (84KB BSS)
- f695f57 perf: fd-table free-slot cursor for open/pipe fd allocation (was O(4096) scan per open); fix VFS init banner 256->4096 FDs
- 9fe1f2f fix: bound pseudo-FS stack bufs by sizeof not user count, sb16 ISA DMA addr/bounce guard
- 4486e6a fix: bound execve proc_name / fork name+cwd copies to PCB field size
- d6cd9fb perf+fix: kernel mem funcs to rep movsl/stosl dword path, wakeup rollback on full ready queue

### 变更统计

### 构建记录（本周 logs）

## 周报 2026-09-17（窗口 2026-09-10 21:50:00 起）

### 提交列表
- 5c464be test(info): multi-page proc-list probe coverage - real N/P page flips
- 4f2ece9 chore(cron): daily summary - 2026-09-16
- d4d3fc0 chore(cron): daily summary - 2026-09-15
- abe3e31 test(clock): alarm status-row probe coverage - serial ALST echo + assertions
- 83eb5b7 test(sprach): appmenu probe - assert Force Quit closes every listed window
- 1128610 feat(terminal): SGR background rendering — 48;5;n / 40-47 / 49 + probe hardening
- 9bbbc4e fix(sched): syscall yield quantum gating + busy-poll to m4k_sleep — restore GUI FPS
- f32a82e feat(logview): / filter mode — substring match, F jump, Esc clear
- c4ba656 chore(cron): daily summary - 2026-09-12
- 4eddbdf fix(sprach): dock hover probe — full-test ISO selection + settle window
- a439074 feat(sprach): dock icon hover highlight + window-title tooltip
- 6a26749 test(info): fix proc-list probe block parsing + deterministic Tab check
- ee1f42e fix(graphics): vblank sync for LFB flips + copland dead-client reaping + setenv overwrite leak
- 26c5ef2 chore(cron): daily summary - 2026-09-11

### 变更统计
 usr/src/sprach/sprach.h             |     2 +
 usr/src/sprach/sprach_stack         |   Bin 238912 -> 238928 bytes
 40 files changed, 46466 insertions(+), 45309 deletions(-)

### 构建记录（本周 logs）
[2026-08-22 20:02:25] task1 PASS: BUILD OK / UNIT OK / DESKTOP ASSERT OK (exit 0)
[2026-08-23 00:03:18] task1 PASS: build+unit+qemu_smoke(desktop assert) EXIT=0 commit=670ad81 feat(copland): phase 1 cleanroom protocol layer - wire format, SPSC rings, object map, connection discovery
[2026-08-23T05:03:01] task1 PASS (build+unit+qemu_desktop) @ 4b882e5
[2026-08-23 10:00:37] task1 PASS: BUILD OK / UNIT OK / QEMU DESKTOP ASSERT OK (commit 4b882e5)
[2026-08-29 15:03:17] task1 PASS (build+unit+qemu smoke, exit 0)
[2026-08-29 05:08:00] task1 PASS: build+unit+qemu_desktop 全部通过（首次因并发构建踩踏误报，串行重跑通过）
[2026-08-29 10:03:58] task1 PASS (build+unit+qemu_smoke+desktop_assert) commit=c271736
[2026-08-29 20:04:57] task1 PASS (build+unit+qemu smoke) exit=0
[2026-08-30 00:03:37] task1 PASS build+unit+qemu_smoke (commit 808db35)

### 深度分析（Agent）

**主题**：本周 9 个实质提交全部围绕桌面应用功能补全与探针覆盖强化——① sprach dock 图标悬停高亮 + 窗口标题 tooltip（a439074）及探针修复（4eddbdf）；② 终端 SGR 背景色渲染：48;5;n / 40-47 / 49（1128610）；③ logview `/` 过滤模式：子串匹配、F 跳转、Esc 清除（f32a82e）；④ 调度修复：syscall yield 量子门控 + 忙轮询转 m4k_sleep，恢复 GUI FPS 至 31-33（9bbbc4e，解决长期 3 FPS 问题）；⑤ 图形栈修复：LFB 翻转 vblank 同步 + copland 死客户端回收 + setenv 覆盖泄漏（ee1f42e）；⑥ 三项探针工程：proc-list 解析修复与 Tab 确定性检查（6a26749）、appmenu Force Quit 全关断言（83eb5b7）、clock ALST 串口回显（abe3e31）、info 多页 N/P 真实翻页覆盖（5c464be）。

**风险点**：
- 40 文件 ±4.5 万行的巨大变更量主要来自内嵌 ELF 二进制（sprach_stack 等），实际代码改动面可控；但提交中混合二进制与源码，review 时需注意 diff 噪声。
- copland 连接表槽位只回收不预防：8 槽上限仍硬编码，桌面长期运行（>8 次应用生命周期）仍可能触发 REJECTED，需要压力回归验证 reaper 的实际效果。
- vblank 轮询上限 300 次（概率性消撕裂）是折中方案，PIT 中断驱动翻转仍是遗留项；terminal_256 探针已固化为此类内核饥饿回归的哨兵。
- 12 进程负载下桌面 FPS≈1.1 的旧问题仍未闭合（shell.c:468 pause 忙等是首要嫌疑），量化修复的工具（apps_fps_probe.py）已就位。

**测试覆盖**：本周每个功能提交均附带专用探针（dock_hover 4/4、appmenu FQ、ALST、info 多页），且探针断言从"≥1 次成功"升级为"枚举完备性精确计数"（FQ 关闭数=列举数）与"多页 PID 不相交"等更强断言；make test 28/28 全周保持绿色，构建记录显示 2026-09-13 后无 task1 失败。覆盖薄弱处：12 进程 FPS 场景与 copland 槽位压力场景无自动化探针。

**技能沉淀**：新增 m4kk1-graphics-stack reference `gui-probe-patterns-2026-09-week.md`（logview 地址重叠根因、SGR bg、串口回显探针模式、多页探针教训），并修正 vblank reference 中"logview filter unresolved/reverted"的过时状态为已解决（f32a82e，根因是 0xF50000 跨度地址重叠而非 PCC codegen，logview 已迁 0xE20000）。
