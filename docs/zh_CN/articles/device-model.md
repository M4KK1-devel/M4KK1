# 设备模型

## 双层架构

M4KK1 采用物理/逻辑分离的双层设备模型:

### 物理层 `/device`

物理设备发现与枚举层，负责:
- 枚举总线上的设备
- 报告设备类型、厂商、功能
- 管理设备电源状态

### 逻辑层 `/dev`

逻辑设备节点层，提供:
- 用户空间可访问的设备文件
- 标准 I/O 接口（open/read/write/close）
- 设备抽象（console、null、zero）

## 当前设备

### console

- 路径: `/dev/console`
- 后端: 串口 COM1 (0x3F8)
- 配置: 38400 8N1，无流控
- 提供阻塞和非阻塞读取

### null

- 路径: `/dev/null`
- 行为: 丢弃所有写入数据
- 读取: 返回 EOF (0 字节)

### zero

- 路径: `/dev/zero`
- 行为: 读取时返回无限 `\0` 字节
- 写入: 丢弃

## 串口控制台详情

文件: `sys/src/drivers/console.c`

### UART 寄存器

| 端口偏移 | 功能 |
|---------|------|
| +0 | 数据寄存器 (R/W) |
| +1 | 中断使能寄存器 |
| +5 | 线路状态寄存器 |
| +5 bit 6 | 发送保持寄存器空 (THR) |
| +5 bit 0 | 数据就绪 (DR) |

### 核心实现

```c
// 发送字符 (忙等待直到 THR 为空)
void ser_putc(char c) {
    while (!(inb(PORT + 5) & 0x20));
    outb(PORT, c);
}

// 非阻塞读取 (-1 = 无数据)
int ser_getc(void) {
    if (inb(PORT + 5) & 0x01)
        return inb(PORT);
    return -1;
}
```

## 驱动开发路线图

| 驱动 | 优先级 | 依赖 |
|------|--------|------|
| PS/2 键盘 | 高 | IRQ1 处理 |
| 帧缓冲 (VBE/VESA) | 高 | 点阵字体 |
| ATA/PATA | 中 | 块缓存 |
| AHCI | 中 | ATA 完成 |
| NE2000 / RTL8139 | 低 | lwIP 集成 |
