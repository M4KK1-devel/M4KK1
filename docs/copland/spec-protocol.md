# Copland 协议规范（spec-protocol）

> 阶段 0 洁净室规范提取。本文档是 Copland 显示服务器协议的**唯一权威定义**。
> 实现阶段（阶段 1-3）只允许依据本文档与另两份规范（消息格式、事件流），
> 不得回看任何参考源码。
>
> 提取自现代显示服务器协议的核心设计思想：对象化异步消息协议 +
> 双缓冲 surface 状态 + 原子提交 + 合成器全权负责输出。

## 0. 设计原则

1. **对象模型**：协议由一组"接口"（interface）组成。每个接口定义若干
   请求（request，客户端→服务器）与事件（event，服务器→客户端）。
   每个协议消息作用于一个对象实例，由对象 ID 标识。
2. **异步消息**：请求与事件都是异步的，没有请求-响应配对。需要屏障时
   使用 `sync` 请求 + 回调对象的 `done` 事件。
3. **双缓冲状态**：surface 的几何/缓冲/损伤都是"待定（pending）"状态，
   只有 `commit` 请求才原子地将其提升为"当前（current）"状态。
4. **合成器主权**：客户端只提交内容（buffer + damage），不决定屏幕上
   的最终位置与层叠；布局由 WM 客户端通过与服务器协商完成。
5. **4P1 适配**（与参考协议的差异，均为 M4KK1 现实约束）：
   - 传输层是共享内存环形缓冲（见 spec-wire-format），非 socket；
   - 缓冲区共享通过**平坦地址**传递（无 MMU，全进程共享地址空间），
     没有 fd 传递；
   - 坐标一律为整数像素（无定点小数类型）；
   - 不定义拖放/剪贴板/子表面/触摸（v1 范围外）；
   - 像素格式仅一种：`xrgb8888`（32bpp，[31:0] X:R:G:B 8:8:8:8，
     与 VESA 线性帧缓冲一致）。

## 1. 接口总表

| 接口 | 角色 | 请求 | 事件 |
|---|---|---|---|
| copland_display | 核心单例（对象 ID 恒为 1） | sync, get_registry | error, delete_id |
| copland_registry | 全局对象注册表 | bind | global, global_remove |
| copland_callback | 一次性回调 | — | done |
| copland_compositor | 合成器单例 global | create_surface, create_region, release | — |
| copland_shm | 共享内存 global | create_pool, release | format |
| copland_shm_pool | 共享内存池 | create_buffer, resize, destroy | — |
| copland_buffer | 内容缓冲 | destroy | release |
| copland_surface | 可显示表面 | destroy, attach, damage, frame, set_opaque_region, set_input_region, commit | enter, leave |
| copland_region | 区域（矩形集合） | add, subtract, destroy | — |
| copland_seat | 输入设备组 global | get_pointer, get_keyboard, release | capabilities |
| copland_pointer | 指针设备 | set_cursor, release | enter, leave, motion, button, axis |
| copland_keyboard | 键盘设备 | release | enter, leave, key, modifiers |
| copland_output | 输出设备 global | release | geometry, mode, scale, done |

服务器发布的 global（registry 中可见）：
`copland_compositor`(name 1)、`copland_shm`(name 2)、`copland_seat`(name 3)、
`copland_output`(name 4)。全部 version 1。

## 2. copland_display（ID 恒为 1，无需创建）

### 请求

**sync(opcode 0)** — 异步往返屏障
- 参数：`callback` (new_id, copland_callback)
- 服务器收到后：先处理完此前收到的所有请求，然后在该回调对象上发
  `done` 事件（callback_data 未定义），随后销毁该回调对象（发 delete_id）。

**get_registry(opcode 1)** — 获取注册表
- 参数：`registry` (new_id, copland_registry)
- 服务器为每个当前存在的 global 发送一次 `global` 事件。

### 事件

**error(opcode 0)** — 致命错误
- 参数：`object_id` (uint)，`code` (uint)，`message` (string)
- 全局错误码：0 invalid_object（对象不存在）、1 invalid_method
  （接口上无此请求或消息畸形）、2 no_memory、3 implementation。

**delete_id(opcode 1)** — 对象 ID 删除确认
- 参数：`id` (uint)
- 服务器不再使用某个客户端创建的对象后发送。客户端收到后方可复用
  该 ID。服务器保证此事件之后不再向该对象发送任何事件。

## 3. copland_registry

### 请求

**bind(opcode 0)**
- 参数：`name` (uint)，`id` (new_id，无接口约束)
- 按名绑定 global。`id` 的新接口即该 global 的接口。
  对无效 name 发 error(invalid_object)。

### 事件

**global(opcode 0)**：`name` (uint)，`interface` (string)，`version` (uint)
**global_remove(opcode 1)**：`name` (uint)

## 4. copland_callback

### 事件

**done(opcode 0)**：`callback_data` (uint)

## 5. copland_compositor

### 请求

**create_surface(opcode 0)**：`id` (new_id, copland_surface)
**create_region(opcode 1)**：`id` (new_id, copland_region)
**release(opcode 2)**：销毁本对象，不影响其它对象。

## 6. copland_shm / copland_shm_pool

### copland_shm

**create_pool(opcode 0)**：`id` (new_id, copland_shm_pool)，
`addr` (uint，池的平坦物理地址)，`size` (int，字节数)
- 替代参考协议的 fd 传递：客户端在自己的 BSS/静态区划出一段内存，
  把**地址与大小**告知服务器，双方直接共享。
- 服务器对 addr/size 不做对齐要求，但合成时按 stride 读取。

**事件 format(opcode 0)**：`format` (uint)
- 绑定时发送。v1 只发一次，值恒为 1（xrgb8888）。

### copland_shm_pool

**create_buffer(opcode 0)**：`id` (new_id, copland_buffer)，`offset` (int)，
`width` (int)，`height` (int)，`stride` (int)，`format` (uint)
- 在池内 offset 字节处创建 w×h、行距 stride 的 buffer。
  format 非 1 时发 error。
**resize(opcode 1)**：`size` (int) — 只允许扩大。
**destroy(opcode 2)**：销毁池。已创建的 buffer 保持有效。

## 7. copland_buffer

### 请求

**destroy(opcode 0)**：销毁 buffer 对象。若已被 attach 且未 commit，
不影响后续 commit 语义。

### 事件

**release(opcode 0)**：服务器不再读取该 buffer 的像素，客户端可复用
 backing store。服务器在把该 buffer 从合成输入中替换下来后发送。

## 8. copland_surface（核心）

### 生命周期

surface 创建后无内容（未映射）。首次 commit 一个非空 buffer 即**映射**
（mapped），此后服务器可能对其进行合成与输入路由。`destroy` 或 commit
空 buffer 使其取消映射。

### 状态与双缓冲

surface 状态分 **pending**（请求修改）与 **current**（合成器使用）：

| 状态 | 初始值 | 修改请求 | commit 语义 |
|---|---|---|---|
| buffer | 无 | attach | 待定 buffer 提升为当前；尺寸=buffer 尺寸 |
| damage | 空 | damage | 待定损伤并集提升为当前损伤（合成后清零） |
| opaque_region | 空 | set_opaque_region | 整体替换 |
| input_region | 无穷 | set_input_region | 整体替换 |

### 请求

**destroy(opcode 0)**：销毁 surface。若当前有 buffer 被 commit，
服务器先发 copland_buffer.release 再回收。

**attach(opcode 1)**：`buffer` (object, copland_buffer, 可空)，`x` (int)，`y` (int)
- 设定待定 buffer。x/y v1 恒为 0（保留参数以对齐参考协议形状）。
- buffer 为空时，下一次 commit 使 surface 取消映射（内容移除）。

**damage(opcode 2)**：`x`, `y`, `width`, `height` (int, surface 局部坐标)
- 待定损伤 = 旧待定损伤 ∪ 此矩形。surface 外的部分被忽略。

**frame(opcode 3)**：`callback` (new_id, copland_callback)
- 请求帧节流提示。下一次 commit 生效；服务器在**该 surface 参与的
  下一次帧合成完成后**发送 done(callback_data=帧时间 ms)。
- 服务器不应对完全不可见的 surface 发送帧回调。

**set_opaque_region(opcode 4)**：`region` (object, copland_region, 可空)
- 不透明区域提示（优化用途）。空 region = 整面可能透明。

**set_input_region(opcode 5)**：`region` (object, copland_region, 可空)
- 可接收指针事件的区域。空 region（NULL）= 无穷（整面收输入）。

**commit(opcode 6)**
- 原子地把 pending 状态提升为 current，形成一个内容更新（CU）。
- CU 排入该 surface 的更新队列，在下一个帧周期统一应用（见
  spec-event-flow §3）。
- 若本次 commit 含新 buffer 且旧 buffer 仍在使用，旧 buffer 的
  release 事件在新 buffer 首次参与合成后发送。

### 事件

**enter(opcode 0)**：`output` (object, copland_output)
**leave(opcode 1)**：`output` (object, copland_output)
- surface 进入/离开某个输出的可见区域时发送。

## 9. copland_region

**add(opcode 1)** / **subtract(opcode 2)**：`x`,`y`,`width`,`height` (int)
**destroy(opcode 0)**：销毁。region 语义在 set_*_region 时被复制。

## 10. copland_seat

### 事件

**capabilities(opcode 0)**：`capabilities` (uint 位掩码)
- bit0 pointer、bit1 keyboard。绑定 seat global 时发送。

### 请求

**get_pointer(opcode 0)**：`id` (new_id, copland_pointer)
**get_keyboard(opcode 1)**：`id` (new_id, copland_keyboard)
- seat 无对应能力时发 error（v1 恒有 pointer+keyboard，不会触发）。

## 11. copland_pointer

### 请求

**set_cursor(opcode 0)**：`serial` (uint)，`surface` (object, 可空)，
`hotspot_x` (int)，`hotspot_y` (int)
- serial 必须匹配最近一次 enter 的 serial，否则忽略。
- v1：surface 恒为空（使用内核硬件光标），仅保留协议形状。

### 事件

所有事件按发生顺序投递；每个事件携带递增 serial。

**enter(opcode 0)**：`serial` (uint)，`surface` (object)，
`surface_x` (int)，`surface_y` (int)
**leave(opcode 1)**：`serial` (uint)，`surface` (object)
  - 先 leave 旧焦点、后 enter 新焦点。
**motion(opcode 2)**：`time` (uint, ms)，`surface_x`，`surface_y` (int)
**button(opcode 3)**：`serial`，`time` (uint)，`button` (uint)，
`state` (uint: 0=released 1=pressed)
- button 编码：0x110 左键（左键为主键），0x111 中键，0x112 右键。
**axis(opcode 4)**：`time` (uint)，`axis` (uint: 0=垂直 1=水平)，
`value` (int，点击数；垂直轴向上为负)

## 12. copland_keyboard

### 事件

**enter(opcode 0)**：`serial` (uint)，`surface` (object)
**leave(opcode 1)**：`serial` (uint)，`surface` (object)
**key(opcode 2)**：`serial` (uint)，`time` (uint)，`key` (uint)，`state` (uint)
- key 为 M4KK1 键盘层产出的**字符码**（非原始扫描码；无独立 keymap
  对象，这是 4P1 简化）。
**modifiers(opcode 3)**：`serial` (uint)，`mods_depressed` (uint)，
`mods_latched` (uint)，`mods_locked` (uint)，`group` (uint)
- M4KK1 仅填 mods_depressed：bit0 shift、bit1 ctrl、bit2 alt。
  latched/locked/group 恒 0。enter 事件后必发一次 modifiers。

## 13. copland_output

### 事件（绑定时按序发送）

**geometry(opcode 0)**：`x`,`y`,`physical_width`,`physical_height`,
`subpixel`,`transform` (int)，`make` (string)，`model` (string)
- M4KK1 单输出：x=y=0，physical 尺寸=像素尺寸，transform=0。
**mode(opcode 1)**：`flags` (uint: 0x3 current|preferred)，
`width`，`height` (int)，`refresh` (int, mHz)
- 800×600，refresh 由服务器按帧率估计。
**scale(opcode 2)**：`factor` (int) = 1
**done(opcode 3)**：以上信息发送完毕的原子边界。

### 请求

**release(opcode 0)**

## 14. 错误处理约定

- 任何 error 事件对客户端是致命的：客户端应销毁连接退出。
- 服务器对无法解析的消息：丢弃当前环中剩余字节到消息边界，发
  error(invalid_method)。
- 服务器对不存在的对象 ID：发 error(invalid_object)，不崩溃。

## 15. WM（Sprach）专用扩展

Sprach 作为**特殊客户端**管理布局。服务器为绑定了 compositor 的
客户端中标记为 WM 的那个（绑定 registry name 1 时携带 WM 旗标的
约定：Sprach 绑定 seat 后第一个调用 set_cursor/或绑定 compositor）
保留布局权：
- v1 布局仍由服务器实现 surface 的屏幕位置（见 spec-event-flow §4
  的位置来源），Sprach 通过创建装饰 surface 并摆放它们来表现窗口
  框架，客户端 surface 的位置由 Sprach 的 MOVE 命令（保留在旧
  cmd 环，见 spec-wire-format §6 兼容层）驱动。

（阶段 3 落地时若需要更细的 WM 协议接口，须先修订本文档。）
