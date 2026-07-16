# M4KK1操作系统

**作者：makkichan947**

**仓库：https://github.com/makkichan947/M4KK1**

M4KK1是一个创新的操作系统项目，由makkichan947一人独立完成，旨在探索现代操作系统设计的可能性。该项目完全从零开始开发，采用独特架构和先进技术，为用户提供高性能、安全、易用的计算环境。

## 🚀 系统特性

- **宏内核架构**：Y4KU宏内核，模块化设计，易于扩展和维护
- **高级文件系统**：YFS文件系统，支持压缩、快照、休眠等特性
- **现代化工具链**：LangCC编译器集合，支持多语言开发
- **完整用户环境**：从文本编辑器到桌面环境的全套工具
- **驱动程序集合**：支持多种硬件设备的驱动程序

## 项目特色

### 🚀 创新架构
- **微内核设计**：采用先进的微内核架构，提供更好的模块化和安全性
- **独特文件系统**：YFS文件系统超越传统设计，支持压缩、快照、休眠等特性
- **全新交换系统**：Swap2交换系统提供压缩、日志、快照等高级功能
- **现代化引导**：Bootcamp引导程序采用创新的双阶段引导方案

### 🛠️ 完整工具链
- **LangCC编译器**：多语言编译器集合，支持C/C++、汇编、Shell等
- **PkgMgr包管理器**：企业级包管理系统，支持原子操作和高级特性
- **现代化Shell**：M4SH Shell提供强大的脚本和交互功能

### 🎨 丰富应用生态
- **VI编辑器**：移植并改进的Vim编辑器，专为M4KK1优化
- **F1le文件管理器**：参考yazi设计的现代化文件管理器
- **Ptdsk分区工具**：先进的图形化分区管理工具
- **网络管理套件**：Ntmgr/TUI和Ntctl/CLI网络管理工具
- **M4man手册浏览器**：先进的TUI手册浏览系统

### 🖥️ 图形界面
- **Copland窗口服务器**：Wayland风格的现代化窗口服务器
- **Rubidium桌面环境**：平铺式桌面环境，参考Hyprland设计

## 系统要求

### 硬件要求
- **处理器**：x86_64架构，支持64位指令集
- **内存**：最小512MB，推荐1GB以上
- **存储**：至少2GB可用空间
- **图形**：支持VGA或更高图形显示

### 软件依赖
- **编译器**：LangCC编译器集合
- **汇编器**：NASM汇编器
- **虚拟机**：QEMU（用于测试）
- **构建工具**：Make、标准C库开发包

## 快速开始

### 环境设置

```bash
# 克隆项目
git clone https://github.com/makkichan947/m4kk1.git
cd m4kk1

# 设置开发环境
make dev-setup

# 检查依赖
make check-deps
```

### 构建系统

```bash
# 构建完整系统
make everything

# 或者使用构建脚本
./scripts/build.sh everything

# 创建ISO镜像
make iso

# 在QEMU中测试
make qemu-test
```

### 运行测试

```bash
# 运行完整测试套件
./test/test.sh

# 或者运行特定测试
./test/test.sh unit          # 单元测试
./test/test.sh integration   # 集成测试
./test/test.sh performance   # 性能测试
```

## 项目结构

```
m4kk1/
├── sys/                    # 系统核心
│   ├── boot/bootcamp/      # Bootcamp引导程序
│   └── src/                # 内核源代码
│       ├── kernel/         # 内核核心
│       ├── fs/             # 文件系统
│       ├── mm/             # 内存管理
│       ├── drivers/        # 设备驱动
│       ├── arch/           # 架构相关
│       └── include/        # 头文件
├── usr/                    # 用户程序
│   ├── bin/                # 用户二进制文件
│   │   ├── vi/             # VI编辑器
│   │   ├── f1le/           # 文件管理器
│   │   ├── ptdsk/          # 分区工具
│   │   ├── ntmgr/          # 网络管理器(TUI)
│   │   ├── ntctl/          # 网络管理器(CLI)
│   │   ├── m4man/          # 手册浏览器
│   │   ├── copland/        # 窗口服务器
│   │   └── rubidium/       # 桌面环境
│   └── opt/                # 可选软件包
│       └── langcc/         # 编译器集合
├── pkg/                    # 包管理系统
│   └── pkgmgr/             # 包管理器
├── test/                   # 测试套件
├── scripts/                # 构建和维护脚本
├── docs/                   # 文档
└── Makefile               # 主构建脚本
```

## 核心组件

### 内核 (Y4KU)
- **微内核架构**：模块化设计，易于扩展和维护
- **内存管理**：先进的虚拟内存和物理内存管理
- **进程调度**：高效的进程调度和上下文切换
- **设备驱动**：统一的设备驱动框架

### 文件系统 (YFS)
- **高级特性**：压缩、快照、休眠、日志
- **性能优化**：异步I/O、智能缓存、预读机制
- **可靠性**：原子操作、事务支持、错误恢复

### 用户界面
- **窗口服务器**：现代化的图形显示基础设施
- **桌面环境**：平铺式窗口管理，丰富的动画效果
- **终端工具**：现代化的命令行工具集合

## 开发指南

### 构建单个组件

```bash
# 构建内核
make kernel

# 构建特定工具
make -C usr/bin/vi all

# 构建所有用户工具
make user-tools
```

### 调试和测试

```bash
# 开发模式构建
make dev

# 运行特定测试
make -C test/unit all

# 性能分析
make -C test/performance all
```

### 贡献代码

1. Fork项目仓库
2. 创建功能分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送分支 (`git push origin feature/amazing-feature`)
5. 创建Pull Request

## 许可证

本项目采用GPL-3.0许可证开源。详见[LICENSE](LICENSE)文件。

## 致谢

感谢以下开源项目和社区的贡献：

- [osdev.org](https://wiki.osdev.org/) - 操作系統开发资源
- [Wayland项目](https://wayland.freedesktop.org/) - 现代化显示服务器协议
- [Linux内核社区](https://kernel.org/) - 开源操作系統典范
- [GNU项目](https://gnu.org/) - 自由软件基金会

## 联系方式

- 项目主页：https://github.com/makkichan947/m4kk1
- 问题反馈：https://github.com/makkichan947/m4kk1/issues
- 邮件列表：m4kk1-devel@lists.example.com

## 免责声明

这是一个实验性操作系统项目，仅供学习和研究使用。请勿在生产环境中使用。本项目不保证任何形式的稳定性、安全性或兼容性。

---

**享受探索M4KK1操作系统的乐趣！** 🎉