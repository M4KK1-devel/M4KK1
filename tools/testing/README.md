# M4KK1 测试套件

## 目录结构

```
tools/testing/
├── README.md               # 本文件
├── target/                 # 目标机测试 (在 M4KK1 上运行)
│   ├── run_all.sh          # 入口：执行所有测试
│   ├── lib/
│   │   └── test_util.m4sh  # 通用测试函数
│   ├── syscall/            # 系统调用测试
│   ├── fs/                 # 文件系统测试
│   ├── proc/               # /sys/proc 测试
│   └── stress/             # 压力测试
├── host/                   # 宿主机测试 (在 WSL/Linux 上运行)
│   ├── check_elf.py        # ELF 格式检查
│   ├── check_style.sh      # C 代码风格检查
│   └── qemu_runner.sh      # QEMU 自动化测试
└── fixtures/               # 测试数据文件
```

## 使用方法

### 在 M4KK1 上运行测试

```sh
cd /tools/testing/target
sh run_all.sh
```

### 在宿主机上运行测试

```sh
cd tools/testing/host
./check_style.sh
./check_elf.py ../../m4kk1.krn
./qemu_runner.sh
```
