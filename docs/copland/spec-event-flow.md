# Copland 事件流文档（spec-event-flow）

> 阶段 0 洁净室规范提取。定义从输入设备到屏幕像素的完整事件路径、
> 帧周期与原子提交时序。实现阶段只依据本文档。

## 1. 总览：两大数据流

```
输入流： 键盘 IRQ ─→ 键盘驱动 ─→ 服务器输入队列 ─→ 焦点路由 ─→ 客户端事件环
输出流： 客户端渲染到 pool ─→ attach/damage/commit ─→ 服务器 pending 状态
         ─→ 帧周期: 应用 CU → 合成(damage 优化) → flip → 帧回调 → buffer release
```

服务器是**唯一**与输入硬件和输出硬件对话的进程。

## 2. 输入路径（设备 → 客户端）

### 2.1 采样

- 键盘：服务器主循环轮询 `m4k_get_keyboard_event`（与现 Sprach 直接
  轮询同一 syscall；迁移后唯一轮询者变为服务器）。
- 指针：服务器轮询 `m4k_get_mouse_event`（QEMU/PS2 鼠标）。
  v1 服务器以固定灵敏度把相对位移换算为绝对坐标（增量累加 + 屏幕
  边界钳制），并调用 `m4k_update_cursor` 移动硬件光标。

### 2.2 焦点模型

- **键盘焦点**：唯一。服务器维护 kbd_focus 指针，指向一个映射中的
  surface。切换时机：鼠标点击（button press 时把 kbd_focus 设为
  点击命中的 surface）+ Alt+Tab 切换窗口 Z 序。
- **指针焦点**：按指针坐标从顶到底找第一个 input_region 命中的
  映射 surface（input_region 为无穷时整个 surface 可命中）。
- 切换时序：先 leave(旧) 后 enter(新)，serial 递增。

### 2.3 指针路由算法（每指针事件）

```
pick(x, y):
    从 z 序顶到底遍历映射 surface:
        if 点在 surface 矩形内 且 input_region 命中:
            return surface
    return NULL
```

- 相对坐标：event.surface_x = pointer.x - surface.x（整数像素）。
- 指针在两 surface 间移动产生 leave+enter 对；同一 surface 内产生
  motion。
- 按钮事件发给当前指针焦点；键盘事件发给 kbd_focus。
- 按钮事件仅路由到指针焦点 surface（隐式提升 z 序的语义由 WM 的
  点击处理决定，v1 服务器做隐式提升）。

### 2.4 键盘路由

- key 事件 → kbd_focus surface 的客户端。
- 服务器先于客户端消费以下保留组合键（global hotkey）：
  - **Alt+Tab**：z 序轮换（提升次顶窗口到顶），不投递给客户端。
  - **Alt+F4**：销毁 kbd_focus surface 的客户端连接（发 error 后
    断开）。v1 用 destroy 连接代替复杂的 close 请求。
- modifiers 事件在 enter 后必发；后续按实际状态变化发送。

#### 2.5 Alt+F4 语义补充
（Alt+F4 会导致无保存的客户端数据丢失——v1 接受，M4KK1 客户端无
持久状态。）

## 3. 帧周期（输出路径）

### 3.1 服务器主循环节拍

无定时器 syscall 可靠使用（timer.c 存在但 v1 不依赖）。帧周期由
主循环节拍 + 工作量自适应空转构成（与现 copland.c 的
`for volatile` 空转 + dirty 检查相同模式）：

```
loop:
    poll_conn_table()          # 新客户端
    for each conn: drain req ring, apply requests
    poll input, route events
    if any CU queued or dirty: composite_frame()
    else: yield()
    watchdog / heartbeat
    frame pacing spin
```

### 3.2 CU（Content Update）应用

commit 请求到达时：
1. pending → current 状态提升在**帧周期开始**统一进行（服务器把
   各 surface 的 CU 从队列取出应用），不在请求解析时立即生效。
2. 应用 = 把新 buffer 设为合成输入、damage 并入帧损伤集、记录
   待释放旧 buffer 列表、登记帧回调列表。
3. 这保证同帧多请求（如 attach+damage+commit 一批）原子生效，
   且同一屏幕区域只合成一次（damage 合并）。

### CU 应用规则（原子提交）
- 一个 CU 含：new buffer（可空）、damage 并集、opaque/input region。
- 帧损伤集 = 所有 CU 的 damage ∪ 因 surface 移动/映射/取消映射/
  z 序变化产生的"旧位置 ∪ 新位置"曝光损伤。
- 曝光损伤同样并入帧损伤集，由重绘背景+重叠面合成覆盖。
- 空 damage 的 commit（仅确认帧节奏）允许，此时帧损伤集不变。

### 3.3 合成（damage 优化）

- 帧合成只遍历与帧损伤集相交的 surface：
  ```
  for surface in z-order (bottom→top):
      r = intersect(surface.rect, frame_damage)
      if empty: skip
      blit surface.buffer 的 r 子矩形到后备缓冲
  背景在最先绘制（渐变，只绘损伤区）
  ```
- 后备缓冲仍是全屏 VESA 双缓冲（m4k_gfx_blit + m4k_flip_rect）。
- 合成完成后 `m4k_flip_rect(damage)` 上屏。
- blit 源 stride 使用 buffer 的 stride（pool create_buffer 指定），
  非 surface 宽度——修复旧代码"传 w 即 stride"的隐患，本规范要求
  stride 显式传递。
- 子矩形 blit 时源指针 = buffer_addr + r.top*stride + r.left*4，
  宽度取 r 的宽度（kernel blit 以传入 w 为 stride——阶段 2 实现
  时传全表面宽并让 kernel 裁剪，或传 r 并需 kernel 支持独立
  stride；见阶段 2 决策记录）。

### 3.4 帧回调与 buffer 释放

帧合成 + flip 完成后：
1. 对本帧登记的所有 frame callback 发 done(帧时间 ms)；
2. 对被替换的旧 buffer 发 release 事件；
3. 清零各 surface 当前 damage（已被合成消费）。

## 3.5 事件时序示例

**窗口 A commit 新帧（完整路径）**：
```
client: attach(buf2); damage(0,0,w,h); frame(cb); commit
server: [帧周期] 应用 CU → 合成 damage 区 → flip → done(cb, t) → release(buf1)
```

**指针从 A 移动到 B**：
```
server: pointer.leave(A), pointer.enter(B, sx, sy), pointer.motion(B, sx, sy)
        [顺序：leave 先于 enter]
```

**键盘焦点切换（点击）**：
```
server: pointer.button(A, press) → kbd.enter(B)? — 不：点击 A 则
        keyboard.leave(旧), keyboard.enter(A)
```

## 4. WM（Sprach）角色与布局

- Sprach 作为 Copland 客户端，拥有三类 surface：
  1. 壁纸/桌面层 surface（最底）；
  2. 每个客户端窗口的**装饰 surface**（由 Sprach 绘制边框/标题栏）；
  3. 任务栏 surface（最顶，非全屏时）。
- 布局流程：客户端 surface 创建并首次映射时，服务器通知 Sprach
  （通过旧 cmd 环保留的 CREATE 通知 → 阶段 3 细化为新协议事件），
  Sprach 决定位置与装饰，通过 MOVE 命令（旧环）摆放客户端 surface。
- v1 完整语义：**服务器持有每个 surface 的屏幕位置**（copland_surface
  的 x/y），Sprach 通过 MOVE 命令驱动。Z 序由服务器维护（映射顺序）
  + Alt+Tab 轮换 + 点击隐式提升。

## 5. 生命周期边角

- 客户端死亡（进程退出）：服务器在连接表发现 disconnect/心跳超时，
  销毁其全部对象：surface unmap（产生曝光损伤）、buffer release、
  对象表清空、连接表槽释放。
- 服务器死亡：客户端轮询 conn_table magic 消失 → 各自退出。
- WM 死亡：现有 watchdog（copland_shm.heartbeat）照常重启 Sprach，
  但 v1 服务器会同时清空**全部** surface（含新协议 surface），Sprach
  重启后重建桌面。子客户端 surface 状态丢失可接受（v1）。
