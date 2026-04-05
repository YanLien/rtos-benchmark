# FreeRTOS AArch64 Benchmark — 编译指南

本目录包含将 rtos-benchmark 移植到 **AArch64 裸机** 上运行 **FreeRTOS** 的全部代码，支持两个目标平台：

| 目标 | CPU | 源码目录 |
|------|-----|----------|
| **QEMU virt** | Cortex-A55 | `src/freertos_aarch64_qemu/` |
| **Orange Pi 5 Plus** | RK3588 Cortex-A55 | `src/freertos_aarch64_orangepi/` |

两个平台各自拥有独立的源码目录，不再使用编译宏切换板级配置。

---

## 目录结构

### QEMU virt (`src/freertos_aarch64_qemu/`)

```
├── board/                          # 板级驱动
│   ├── qemu_virt.h                 # QEMU virt 硬件地址定义
│   ├── pl011_uart.c / .h           # PL011 UART 驱动
│   ├── gicv3.c / .h                # GICv3 中断控制器驱动
│   └── generic_timer.c             # ARM Generic Timer tick 中断
├── arch/aarch64/
│   └── arch_util.c                 # CNTPCT_EL0 高精度计时
├── timer/
│   └── bench_generic_timer.c       # EL1 Physical Timer (中断延迟测试)
├── port/                           # 自定义 FreeRTOS AArch64 port
│   ├── portmacro.h, port.c, portASM.S
├── bare_metal.c, arch_api.h
├── bench_porting_layer_aarch64.c / .h
├── FreeRTOSConfig.h
├── startup_aarch64.S
├── qemu_virt_aarch64.ld
├── freertos_aarch64_qemu.cmake
└── CMakeLists.txt
```

### Orange Pi 5 Plus (`src/freertos_aarch64_orangepi/`)

```
├── board/                          # 板级驱动
│   ├── orange_pi_5.h               # RK3588 硬件地址定义
│   ├── uart_16550.c / .h           # 16550 UART 驱动
│   ├── gicv3.c / .h                # GICv3 中断控制器驱动
│   └── rk3588_timer.c              # ARM Generic Timer tick 中断
├── arch/aarch64/
│   └── arch_util.c
├── timer/
│   └── bench_generic_timer.c
├── port/
│   ├── portmacro.h, port.c, portASM.S
├── bare_metal.c, arch_api.h
├── bench_porting_layer_aarch64.c / .h
├── FreeRTOSConfig.h
├── startup_aarch64.S
├── rk3588_aarch64.ld
├── freertos_aarch64_orangepi.cmake
└── CMakeLists.txt
```

---

## 前置条件（通用）

### 1. 交叉工具链

```bash
sudo apt install gcc-aarch64-linux-gnu
aarch64-linux-gnu-gcc --version
```

### 2. FreeRTOS 内核源码

```bash
cd /path/to/crates
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git
```

**注意**：我们使用自定义的 GICv3 port（各自 `port/` 目录），不使用 FreeRTOS-Kernel 自带的 `portable/GCC/ARM_AARCH64/`（它只支持 GICv2 MMIO 访问）。

---

# 一、QEMU virt（Cortex-A55）

用于开发验证和快速迭代，支持完整的串口输出。

## 硬件配置

| 参数 | 值 |
|------|-----|
| 机器型号 | `qemu-system-aarch64 -M virt,gic-version=3` |
| CPU | Cortex-A55 |
| UART | PL011 (基址 `0x09000000`) |
| GIC | GICv3 (Distributor `0x08000000`, Redist `0x080A0000`) |
| Timer 频率 | 62.5 MHz (CNTFRQ_EL0) |
| RAM 起始 | `0x40000000` |

## 编译

### 方法一：构建脚本（推荐）

```bash
bash scripts/build_freertos_aarch64_qemu.sh -k /path/to/FreeRTOS-Kernel

# 编译后立即运行
bash scripts/build_freertos_aarch64_qemu.sh -k /path/to/FreeRTOS-Kernel -r

# 清理后重新编译
bash scripts/build_freertos_aarch64_qemu.sh -k /path/to/FreeRTOS-Kernel -c
```

### 方法二：CMake

```bash
cmake -GNinja \
    -DRTOS=freertos_aarch64_qemu \
    -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel \
    -S . -B build/qemu_virt
ninja -C build/qemu_virt
```

## 编译输出

```
ELF: build/qemu_virt/freertos_aarch64_qemu.elf
BIN: build/qemu_virt/freertos_aarch64_qemu.bin
MAP: build/qemu_virt/freertos_aarch64_qemu.map
```

## 运行

```bash
qemu-system-aarch64 \
    -M virt,gic-version=3 \
    -cpu cortex-a55 \
    -smp 1 \
    -m 256 \
    -nographic \
    -kernel build/qemu_virt/freertos_aarch64_qemu.elf
```

按 `Ctrl-A X` 退出 QEMU。

---

# 二、Orange Pi 5 Plus（RK3588，Cortex-A55）

用于真机裸机 AMP 性能测试。FreeRTOS 运行在 Cortex-A55 小核上，通过 u-boot 或 remoteproc 启动。

## 硬件配置

| 参数 | 值 |
|------|-----|
| SoC | RK3588 (4x Cortex-A76 + 4x Cortex-A55) |
| 目标核心 | Cortex-A55 (小核) |
| UART | UART2 16550 (基址 `0xFEB50000`) |
| GIC | GIC-600 GICv3 (Distributor `0xFD000000`, Redist `0xFD100000`) |
| Timer 频率 | 24 MHz |
| CPU 主频 | 1.8 GHz |
| 加载地址 | `0x00200000` (预留 16MB) |
| 波特率 | 1500000 |

## 编译

### 方法一：构建脚本（推荐）

```bash
bash scripts/build_freertos_aarch64_orangepi.sh -k /path/to/FreeRTOS-Kernel

# 清理后重新编译
bash scripts/build_freertos_aarch64_orangepi.sh -k /path/to/FreeRTOS-Kernel -c
```

### 方法二：CMake

```bash
cmake -GNinja \
    -DRTOS=freertos_aarch64_orangepi \
    -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel \
    -S . -B build/rk3588
ninja -C build/rk3588
```

## 编译输出

```
ELF: build/rk3588/freertos_aarch64_orangepi.elf
BIN: build/rk3588/freertos_aarch64_orangepi.bin
MAP: build/rk3588/freertos_aarch64_orangepi.map
```

## 真机部署

### 方法一：u-boot 直接加载

1. 将 `freertos_aarch64_orangepi.bin` 复制到 SD 卡或 TFTP 服务器

2. 进入 u-boot 命令行（串口按 Ctrl+C 中断启动）

3. 加载并启动：
   ```
   => load mmc 1:1 0x00200000 freertos_aarch64_orangepi.bin
   => cpu 4 release 0x00200000
   ```

4. 通过调试串口查看输出：
   ```bash
   minicom -D /dev/ttyUSB0 -b 1500000
   ```

### 方法二：Linux remoteproc

```bash
sudo cp freertos_aarch64_orangepi.bin /lib/firmware/
echo freertos_aarch64_orangepi.bin > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state
```

## 调试

### GDB 远程调试

```bash
openocd -f interface/cmsis-dap.cfg -f target/rockchip_rk3588.cfg
aarch64-linux-gnu-gdb build/rk3588/freertos_aarch64_orangepi.elf
(gdb) target remote :3333
```

### 反汇编查看

```bash
aarch64-linux-gnu-objdump -d build/rk3588/freertos_aarch64_orangepi.elf | less
aarch64-linux-gnu-nm build/rk3588/freertos_aarch64_orangepi.elf | sort
```

---

# 共享信息

## 支持的 Benchmark 测试

| 测试 | 说明 | 对应文件 |
|------|------|----------|
| `thread` | 线程创建/销毁 | `bench_thread_test.c` |
| `thread_switch_yield` | 线程切换让步 | `bench_thread_switch_yield_test.c` |
| `mutex_lock_unlock` | 互斥锁加/解锁 | `bench_mutex_lock_unlock_test.c` |
| `sem_context_switch` | 信号量上下文切换 | `bench_sem_context_switch_test.c` |
| `sem_signal_release` | 信号量信号释放 | `bench_sem_signal_release_test.c` |
| `malloc_free` | 内存分配/释放 | `bench_malloc_free_test.c` |
| `message_queue` | 消息队列 | `bench_message_queue_test.c` |
| `interrupt_latency` | 中断延迟 | `bench_interrupt_latency_test.c` |

## 平台差异对比

| 方面 | QEMU virt | Orange Pi 5 Plus (RK3588) |
|------|-----------|--------------------------|
| 源码目录 | `src/freertos_aarch64_qemu/` | `src/freertos_aarch64_orangepi/` |
| CPU | Cortex-A55 (模拟) | Cortex-A55 (物理小核) |
| UART | PL011 (`0x09000000`) | 16550 (`0xFEB50000`) |
| 波特率 | 115200 | 1500000 |
| GIC Distributor | `0x08000000` | `0xFD000000` |
| GIC Redistributor | `0x080A0000` | `0xFD100000` |
| Timer 频率 | 62.5 MHz | 24 MHz |
| CPU 时钟 | 62.5 MHz | 1.8 GHz |
| RAM 起始 | `0x40000000` | `0x00200000` |
| 链接脚本 | `qemu_virt_aarch64.ld` | `rk3588_aarch64.ld` |
| CMake 目标 | `freertos_aarch64_qemu` | `freertos_aarch64_orangepi` |
| 构建脚本 | `build_freertos_aarch64_qemu.sh` | `build_freertos_aarch64_orangepi.sh` |
| 输出目录 | `build/qemu_virt/` | `build/rk3588/` |
| 默认迭代数 | 100 | 10000 |

## 与标准 FreeRTOS AArch64 Port 的区别

| 方面 | 标准 ARM_AARCH64 port | 本项目自定义 port |
|------|----------------------|-------------------|
| GIC 访问 | 内存映射 (MMIO) | 系统寄存器 (ICC_*_EL1) |
| 执行级别 | EL3 | EL3 → EL1 降级 |
| 时钟源 | 外部 timer IP | ARM Generic Timer |
| 上下文切换 | 标准 | GICv3 SGI/IRQ 感知 |

## 故障排除

### 编译错误: `unknown type name 'bool'`
确保编译标志包含 `-include stdbool.h`。

### 编译错误: `unknown value 'cortex-a55' for '-mcpu'`
更新工具链版本。GCC 9+ 支持 Cortex-A55。

### 链接错误: `undefined reference to 'memset'`
确保 `bare_metal.c` 被编译并链接。

### 链接错误: `undefined reference to 'main'`
确保 `bench_all.c` 被编译并链接（它包含 `main()` 函数）。

### QEMU 无输出
确认使用了 QEMU 专用的源码目录 (`src/freertos_aarch64_qemu/`)，其中包含 PL011 UART 驱动。

### 真机无串口输出
1. 确认串口线连接正确（UART2, GPIO 引脚）
2. 确认波特率（默认 1500000）
3. 检查 `orange_pi_5.h` 中的 `UART2_BASE` 地址是否与实际板子匹配
4. 确认 u-boot 已正确将核心释放到 FreeRTOS 入口地址
