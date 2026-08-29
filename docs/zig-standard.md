# M4KK1 Zig 规范 v1.0

适用范围：M4KK1 仓库内所有 Zig 源码（`.zig`）。目标平台：i386
freestanding（无 MMU、无 libc、无 lib Zig），由 GNU ld 链接进现有
ELF 构建链。版本基线：Zig 0.13.0。

## 1. 定位与边界

- Zig 用于**用户态性能敏感模块**（像素原语、行级 blit、字体栅格化）
  与**逻辑复杂度高的纯计算模块**（解析、LUT 生成）。
- 内核侧热路径（`rep movsl` blit、`rep stosl` 梯度、IRQ 驱动鼠标）
  已是手工优化汇编路线，**禁止**用 Zig 重写内核。
- C 与 Zig 以**目标文件（.o）为边界**：Zig `build-obj` 产出 ELF
  i386 目标文件，由 `build_krn.sh` 既有的 `$LD -m elf_i386` 链接。
  禁止 Zig 自带链接器接管链接，禁止生成 PIE。

## 2. 构建

```bash
# 编译（host 上执行，目标 i386 freestanding）
# -mcpu "baseline-sse-sse2"：禁用 SSE —— QEMU i386 默认 CPU 无 SSE，
# Zig 的 memset 内建会编出 movd/movups，guest 直接 #NP(vec=23)。
zig build-obj usr/src/sprach/spr_draw.zig \
    -target x86-freestanding -mcpu "baseline-sse-sse2" \
    -O ReleaseFast -femit-bin=usr/src/sprach/spr_draw.o
```

- 构建集成点：`tools/build/build_krn.sh` 在 C 编译段之前调用上述
  命令（Zig 先行，C 侧消费符号）。
- `.zig` 源文件不进 git LFS，不进 ISO；只有 `.o` 参与链接。
- 工具链路径：`~/zig-0.13.0/zig`（用户态安装，非系统包）。

## 3. C ABI 边界

- 导出函数一律 `export fn`，命名 `z<module>_<name>`（如
  `zsp_draw_str`），参数类型只用 `i32` / `u32` / 裸指针
  （`[*]u32`、`[*:0]const u8`）。
- 禁止跨边界传递 Zig slice/optional/error union/struct-by-value
  （布局不稳定）；复合数据用指针 + 长度参数。
- 共享数据（如 `font5x7`）在**一侧定义**（C 或 Zig），另一侧
  `extern` 声明，同 ELF 内符号解析。
- 字符串跨界一律 `[*:0]const u8`（NUL 终止），不用 Zig slice。

## 4. 语义等价（验收标准）

- 每个 Zig 导出函数必须与 C 原版**字节等价**：给定相同输入，
  产出的像素缓冲完全一致。
- 验收工具：`tools/build/zig_equivalence_test.py` —— 同一构建里
  分别调用 C 与 Zig 实现，逐字节比较缓冲。
- C 原版在替换期间保留（`#ifdef USE_ZIG_DRAW` 切换），等价性
  验证通过并稳定一个发布周期后才允许删除 C 版本。

## 4a. 字体数据共享（font5x7 特例）

`font5x7` 数组定义在 C 侧（`sprach.c`）。Zig 侧 `extern const
font5x7: [96*7]u8` 引用同一符号。**改动字形表必须同步两侧布局**
（96 字符 × 7 行，行序一致）。等价性测试覆盖字形渲染。

## 5. 语言约束

- 无 lib Zig：不 `@import("std")` 产生运行时依赖（`std.testing`
  仅限 host 侧 `zig test` 自测，目标构建禁用）。
- 禁止 panic 路径进目标：所有 `export fn` 不得触发
  `unreachable`/溢出检查失败——用 `+%`/`-%`/`@min`/`@max` 显式
  包裹算术，边界输入（负数、极大值）必须先 clamp 后索引。
- 分配器禁用：Zig 侧不申请内存，只操作调用者传入的缓冲。
- 内联汇编可用但需注释等效 C 原语（对照 musr_fill32）。

## 5a. 编译期求值（comptime）规范

comptime 是 Zig 的核心优势，但目标产物必须可验证：

- LUT、字形表、常量结构**优先 comptime 生成**，产物必须是
  `const` 全局（进 .rodata，无运行时初始化开销）。
- comptime 代码不得调用 `export fn`（避免验证盲区）；comptime
  与运行时路径共享的纯函数，用 `zig test` 断言两者一致。
- 等价性测试必须覆盖 comptime 生成物（与 C 侧静态表逐字节
  对比，见 `zig_equivalence_test.py` 的表比对段）。

## 6. 测试

- 每个 `.zig` 文件内嵌 `test` 块（host 跑 `zig test`，native
  target，允许 std.testing）。
- CI 门槛：`zig test` 全过 + `zig_equivalence_test.py` PASS +
  `make test` 28/0。
- 提交前手动：`build_neticons.sh` 全量构建 + QEMU 交互回归
  （desktop_interact2.py 8/8）。

## 7. 代码风格

- 缩进 4 空格（Zig 官方惯例，与内核 Tab-8 区分）。
- 导出函数上必须注释 C 原型（如 `/// sp_rect equivalent: ...`）。
- 文件头 `//!` 模块注释：用途、C 边界、等价性验收指针。
- 命名：文件 `snake_case.zig`；导出符号 `z<module>_` 前缀；
  内部函数驼峰禁止，一律 snake_case。

## 8. 文档同步义务

- 新增/修改 Zig 模块必须同步更新：本规范（若引入新模式）、
  `README.md` 构建段（工具链要求）、受影响 man 页。
- 每个已落地模块在本文件末尾「模块清单」登记。

## 模块清单

| 模块 | C 边界 | 状态 |
|---|---|---|
| `usr/src/sprach/spr_draw.zig` | sprach.c（sp_* 原语） | v1 已落地：等价性 PASS + 交互 8/8 + make test 28/0（2026-08-29） |
