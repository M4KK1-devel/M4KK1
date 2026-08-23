# Copland 消息格式规范（spec-wire-format）

> 阶段 0 洁净室规范提取。定义 Copland 协议的异步消息线格式、
> 对象 ID 分配规则、以及在 4P1 共享内存环形缓冲上的传输层映射。
> 实现阶段只依据本文档。

## 1. 基本单元

- 字节序：**小端**（与 x86 一致）。
- 对齐：所有字段 4 字节对齐，消息总长按 4 字节向上取整。
- 基本类型表：

| 类型 | 尺寸 | 编码 |
|---|---|---|
| uint | 4B | 无符号 32 位 |
| int | 4B | 有符号 32 位（补码） |
| fixed | — | v1 不使用（整数坐标） |
| object | 4B | 对象 ID（uint） |
| new_id | 4B | 客户端分配的新对象 ID |
| string | 4B+len | 长度字（字节数，含结尾 NUL，NUL 必须存在）+ 内容 + NUL + 0 填充至 4B 倍数。长度 0 = NULL 字符串 |
| array | 4B+len | 长度字（元素字节数）+ 内容 + 0 填充至 4B 倍数 |

## 2. 消息头（8 字节，每条消息前缀）

```
word0: object-id (uint32)
word1: (size-byte << 16) | (opcode & 0xFFFF)
```

- `object-id`：本消息的目标对象（请求）或来源对象（事件）。
- `size-byte`：**整条消息**的字节数（含 8B 头自身），最大 4088。
- `opcode`：该对象接口内的操作码，0..65535。
- word1 的低 16 位是 opcode，高 16 位是 size（字节）。

## 3. 请求与事件的序列化示例

**surface.damage(10, 20, 100, 200)**（对象 7，opcode 2）：
```
07 00 00 00   object-id = 7
18 00 02 00   size=0x18(24B), opcode=2
0A 00 00 00   x = 10
14 00 00 00   y = 20
64 00 00 00   width = 100
C8 00 00 00   height = 200
```

**display.sync(callback=5)**（对象 1，opcode 0）：
```
01 00 00 00
0C 00 00 00   size=12, opcode=0
05 00 00 00   callback new_id = 5
```

**registry.global(name=1, interface="copland_compositor", version=1)**（对象 3，opcode 0） 事件：
string 总长 = 4B 长度字 + 内容+NUL（19B）填充到 20B = 24B；整条 = 8+4+24+4 = **40 (0x28)**：
```
03 00 00 00   object-id = 3
28 00 00 00   size = 0x28 (40B), opcode = 0
01 00 00 00   name = 1
13 00 00 00   string len = 19 ("copland_compositor" 18B + NUL)
"copland_compositor\0" + 1B pad
01 00 00 00   version = 1
```

## 2.x ID 分配规则

（以下规则提取自现代对象协议的通行实践，与参考实现等价。）

- 对象 ID 空间：客户端创建的对象从 **1** 开始递增；服务器创建的
  对象（如 registry global 广播的绑定结果、frame callback 池）使用
  **0xFF00_0000 起的高位段**。
  - 精确规则：客户端 ID ∈ [1, 0xFEFFFFFF]；服务器 ID ∈
    [0xFF000000, 0xFFFFFFFF]。ID 0 无效。
- new_id 由**消息发送方**分配并写入消息（客户端请求带 new_id、
  服务器事件带 new_id 亦然）。
- 对象销毁：客户端 destroy 请求后客户端即认为死；服务器确认后
  （发 delete_id）客户端才可复用该 ID。服务器事件 new_id 创建的
  对象（v1 无此形态）由服务器销毁并发 delete_id。
- copland_display 对象 ID 恒为 **1**，无需创建。

## 4. 传输层：4P1 共享内存环

### 4.1 通道结构

每个客户端与服务器之间有**两个单生产者/单消费者环**：

```
struct copland_conn {
    uint32_t magic;          /* 'CCON' */
    uint32_t version;        /* 1 */
    uint32_t client_id;      /* 服务器分配，从 1 递增 */
    请求环（client→server）:
    uint32_t req_head;       /* 消费者(server)推进 */
    uint32_t req_tail;       /* 生产者(client)推进 */
    uint32_t req_size;       /* 环容量，字节数，2 的幂 */
    uint32_t req_data[req_size/4];
    事件环（server→client）:
    uint32_t evt_head;       /* 消费者(client)推进 */
    evt_tail;                /* 生产者(server)推进 */
    evt_size;                /* 2 的幂 */
    uint32_t evt_data[...];
    辅助:
    uint32_t server_pid;     /* 服务器写 */
    uint32_t client_pid;     /* 守护发现用（调试） */
    uint32_t disconnect;     /* 1=客户端请求断开 */
    uint32_t pad;
};
```

- 生产者写 `tail`，消费者推进 `head`，二者只写自己的指针，只读对方
  的指针（SPSC 无锁约定，与现有 cmd_ring 相同）。
- 环为**字节环**（head/tail 是字节偏移，自然回绕），消息以完整字节
  序列写入，消费者按头部长度字段切分消息。
- 写入协议：生产者先写数据、后推进 tail（发布屏障 = tail 更新）。
  消费者按 head→tail 之间的字节流解析，遇到不足一条完整消息的尾部
  时停下等待。发布顺序约定与现有 copland_shm 的 in_use 发布屏障
  同理。

### 4.2 连接发现与建立

M4KK1 无 socket。发现机制：
- 服务器在固定共享地址 `COPLAND_CONN_BASE`（0x00720000，紧邻现有
  0x00700000 copland_shm / 0x00710000 term_mailbox）放置一张
  **连接表**：

```
struct copland_conn_table {
    uint32_t magic;      /* 'CTBL' */
    uint32_t version;
    uint   slot_count;   /* 8 */
    struct {
        uint32_t in_use;
        uint32_t client_pid;
        uint32_t conn_addr;    /* struct copland_conn 的平坦地址 */
    } slots[8];
};
```

- 新客户端（含 Sprach）启动时：扫描表找 `in_use==0` 槽，**先填
  conn_addr 指向自己的静态 conn 结构、client_pid，最后写 in_use=1
  发布**（发布屏障约定，同 copland_shm）。服务器在主循环里轮询
  表发现新连接。
- 消息不含 fd；buffer 通过 copland_shm.create_pool(addr,size) 的
  平坦地址共享。

### 4.3 flush 语义

- v1 客户端 flush = 无操作（直接写环即对服务器可见）；服务器在
  每轮主循环读取请求环。缺省 nodelay。
- 服务器事件写入事件环，客户端主循环轮询。

## 5. 消息解析与调度（服务器侧）

服务器主循环每轮：
1. 轮询连接表发现新客户端；
2. 对每个活跃连接，从 req 环解析完整消息（按 8B 头 + size 切分）；
3. 按对象 ID 查对象表（见 spec-wire-format §6 对象表）→ 得接口与
   请求分发表；
4. 参数依接口签名反序列化；对 new_id 在对象表登记新对象；
5. 调用该接口的请求处理函数；处理函数内可向事件环写事件。

## 6. 对象表

服务器维护每连接一张对象表：ID → (interface, backing state)。
对象表用**开放寻址哈希**或按需增长的简单数组（ID 稀疏，但 v1 规模
小：每客户端 ≤ 数十个对象）。

## 7. 兼容层（阶段 3 Sprach 迁移期）

旧 `copland_shm` 的 surface 表 + cmd 环**保留**（sprach 与旧终端
客户端继续使用，等价于"旧协议通道"）。新 Copland 协议栈与旧通道
并存：
- 新协议的 surface 与旧 surface 表通过 slot 映射（copland_surface
  对象的 backing 指向 copland_shm.surfaces[i]）。
- 阉割版迁移：sprach 逐步把窗口框架 surface 迁到新协议，最终只留
  新协议。本文档不定义旧通道内部（见 libcopland.h）。
