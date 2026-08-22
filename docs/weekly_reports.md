
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
