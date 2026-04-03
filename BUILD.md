# FreeRTOS on RK3588 (AArch64) — 编译指南

## 概述

本目录包含将 rtos-benchmark 移植到 **RK3588 (Orange Pi 5)** 上运行 **FreeRTOS** 裸机 AMP 的全部代码。FreeRTOS 运行在 Cortex-A76 大核上，通过 u-boot 或 remoteproc 启动。

---

## 目录结构

```
src/freertos_aarch64/
├── board/                          # 板级驱动
│   ├── orange_pi_5.h               # RK3588 硬件地址定义
│   ├── uart_16550.c / .h           # 16550 UART 驱动 (调试输出)
│   ├── gicv3.c / .h                # GIC-600 (GICv3) 中断控制器驱动
│   └── rk3588_timer.c              # ARM Generic Timer tick 中断设置
├── arch/
│   └── aarch64/
│       └── arch_util.c             # CNTPCT_EL0 高精度计时
├── timer/
│   └── bench_generic_timer.c       # EL1 Physical Timer (中断延迟测试)
├── port/                           # 自定义 FreeRTOS AArch64 port
│   ├── portmacro.h                 # 类型定义、临界区、中断掩码宏
│   ├── port.c                      # 任务栈初始化、调度器启动、中断管理
│   └── portASM.S                   # 上下文切换、IRQ 处理 (GICv3 系统寄存器)
├── bare_metal.c                    # memset/memcpy/memmove/assert 裸金属实现
├── bench_porting_layer_aarch64.c   # FreeRTOS 移植层
├── bench_porting_layer_aarch64.h   # 移植层头文件
├── FreeRTOSConfig.h                # FreeRTOS 内核配置
├── startup_aarch64.S               # 启动代码 (EL3→EL1, 向量表, BSS 清零)
├── rk3588_aarch64.ld               # AArch64 链接脚本
├── freertos_aarch64.cmake          # CMake 构建配置
└── CMakeLists.txt                  # 子目录构建
```

---

## 前置条件

### 1. 交叉工具链

安装 `aarch64-linux-gnu-gcc`（Ubuntu/Debian）：

```bash
sudo apt install gcc-aarch64-linux-gnu
```

或使用裸金属工具链 `aarch64-none-elf-gcc`（需单独下载）。

验证安装：

```bash
aarch64-linux-gnu-gcc --version
```

### 2. FreeRTOS 内核源码

```bash
# 在项目根目录的上级 crates/ 目录下
cd /path/to/crates
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git
```

**注意**：我们使用自定义的 GICv3 port（`src/freertos_aarch64/port/`），不使用 FreeRTOS-Kernel 自带的 `portable/GCC/ARM_AARCH64/`（它只支持 GICv2 MMIO 访问，不兼容 RK3588 的 GIC-600）。

### 3. QEMU (可选，用于验证编译)

```bash
# Ubuntu
sudo apt install qemu-system-arm

# 或从源码编译
wget https://download.qemu.org/qemu-10.2.0.tar.xz
tar xf qemu-10.2.0.tar.xz && cd qemu-10.2.0
./configure --target-list=aarch64-softmmu && make -j$(nproc)
```

---

## 编译

### 方法一：使用构建脚本（推荐）

```bash
cd /path/to/rtos-benchmark

# 指定 FreeRTOS-Kernel 路径
bash scripts/build_freertos_aarch64.sh -k /path/to/FreeRTOS-Kernel

# 或使用环境变量
export FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
bash scripts/build_freertos_aarch64.sh

# 清理后重新编译
bash scripts/build_freertos_aarch64.sh -k /path/to/FreeRTOS-Kernel -c

# 查看帮助
bash scripts/build_freertos_aarch64.sh -h
```

### 方法二：手动编译

```bash
# 设置路径
CC=aarch64-linux-gnu-gcc
KERNEL=/path/to/FreeRTOS-Kernel
PROJ=/path/to/rtos-benchmark
SRC=${PROJ}/src/freertos_aarch64
BD=/tmp/rk3588_build

mkdir -p $BD

# 编译标志
CF="-mcpu=cortex-a76 -mgeneral-regs-only -ffreestanding -nostdlib"
CF="$CF -Wall -Wno-unused-parameter -Wno-unused-variable"
CF="$CF -include stdbool.h"
CF="$CF -DFREERTOS_AARCH64"
CF="$CF -DSYS_CLOCK_HW_CYCLES_PER_SEC=1800000000"
CF="$CF -DITERATIONS=10000 -DCALIBRATION_LOOPS=10000"
CF="$CF -I${PROJ}/h -I${SRC} -I${SRC}/board -I${SRC}/port -I${KERNEL}/include"

cd $PROJ

# === 1. 启动代码 ===
$CC $CF -c ${SRC}/startup_aarch64.S       -o $BD/startup.o
$CC $CF -c ${SRC}/port/portASM.S           -o $BD/portASM.o
$CC $CF -c ${SRC}/port/port.c              -o $BD/port.o

# === 2. 板级驱动 ===
$CC $CF -c ${SRC}/board/uart_16550.c       -o $BD/uart_16550.o
$CC $CF -c ${SRC}/board/gicv3.c            -o $BD/gicv3.o
$CC $CF -c ${SRC}/board/rk3588_timer.c     -o $BD/rk3588_timer.o

# === 3. 架构层 & Timer benchmark ===
$CC $CF -c ${SRC}/arch/aarch64/arch_util.c -o $BD/arch_util.o
$CC $CF -c ${SRC}/timer/bench_generic_timer.c -o $BD/bench_generic_timer.o

# === 4. 移植层 & 裸金属支持 ===
$CC $CF -c ${SRC}/bench_porting_layer_aarch64.c -o $BD/bench_porting.o
$CC $CF -c ${SRC}/bare_metal.c             -o $BD/bare_metal.o

# === 5. FreeRTOS 内核 ===
$CC $CF -c ${KERNEL}/tasks.c               -o $BD/tasks.o
$CC $CF -c ${KERNEL}/queue.c               -o $BD/queue.o
$CC $CF -c ${KERNEL}/list.c                -o $BD/list.o
$CC $CF -c ${KERNEL}/timers.c              -o $BD/timers.o
$CC $CF -c ${KERNEL}/stream_buffer.c       -o $BD/stream_buffer.o
$CC $CF -c ${KERNEL}/event_groups.c        -o $BD/event_groups.o

# === 6. Benchmark 框架 ===
$CC $CF -c src/common/bench_utils.c        -o $BD/bench_utils.o
$CC $CF -c src/common/bench_all.c          -o $BD/bench_all.o

# === 7. 所有 benchmark 测试 ===
$CC $CF -c src/common/bench_thread_test.c                    -o $BD/bench_thread_test.o
$CC $CF -c src/common/bench_malloc_free_test.c               -o $BD/bench_malloc_free_test.o
$CC $CF -c src/common/bench_message_queue_test.c             -o $BD/bench_message_queue_test.o
$CC $CF -c src/common/bench_mutex_lock_unlock_test.c         -o $BD/bench_mutex_lock_unlock_test.o
$CC $CF -c src/common/bench_sem_context_switch_test.c        -o $BD/bench_sem_context_switch_test.o
$CC $CF -c src/common/bench_sem_signal_release_test.c        -o $BD/bench_sem_signal_release_test.o
$CC $CF -c src/common/bench_thread_switch_yield_test.c       -o $BD/bench_thread_switch_yield_test.o
$CC $CF -c src/common/bench_interrupt_latency_test.c         -o $BD/bench_interrupt_latency_test.o

# === 链接 ===
$CC -nostdlib -Wl,--no-warn-rwx-segments \
    -T ${SRC}/rk3588_aarch64.ld \
    $BD/startup.o $BD/portASM.o $BD/port.o \
    $BD/uart_16550.o $BD/gicv3.o $BD/rk3588_timer.o \
    $BD/arch_util.o $BD/bench_generic_timer.o \
    $BD/bench_porting.o $BD/bare_metal.o \
    $BD/tasks.o $BD/queue.o $BD/list.o $BD/timers.o \
    $BD/stream_buffer.o $BD/event_groups.o \
    $BD/bench_utils.o $BD/bench_all.o \
    $BD/bench_thread_test.o \
    $BD/bench_malloc_free_test.o \
    $BD/bench_message_queue_test.o \
    $BD/bench_mutex_lock_unlock_test.o \
    $BD/bench_sem_context_switch_test.o \
    $BD/bench_sem_signal_release_test.o \
    $BD/bench_thread_switch_yield_test.o \
    $BD/bench_interrupt_latency_test.o \
    -lgcc \
    
    -o $BD/freertos_aarch64.elf \
    -Wl,-Map=$BD/freertos_aarch64.map

# 生成裸机 bin
aarch64-linux-gnu-objcopy -O binary \
    $BD/freertos_aarch64.elf \
    $BD/freertos_aarch64.bin
```

### 方法三：CMake

```bash
cmake -GNinja \
    -DRTOS=freertos_aarch64 \
    -DFREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel \
    -S . -B build
ninja -C build
```

---

## 编译输出

```
ELF: /tmp/rk3588_build/freertos_aarch64.elf
BIN: /tmp/rk3588_build/freertos_aarch64.bin (~74KB)
MAP: /tmp/rk3588_build/freertos_aarch64.map
```

查看大小和段：

```bash
aarch64-linux-gnu-size /tmp/rk3588_build/freertos_aarch64.elf
```

输出示例：

```
   text    data     bss     dec     hex  filename
  73871     400  185312  259583   3f5ff  freertos_aarch64.elf
```

---

## QEMU 验证（仅编译验证）

> 注意：QEMU `virt` 机器的 UART 基地址 (0x9000000 PL011) 与 RK3588 的 (0xFEB50000 16550) 不同，
> 因此 **QEMU 无法看到输出**。QEMU 验证仅确认二进制能加载和运行（不崩溃）。

```bash
qemu-system-aarch64 \
    -M virt,gic-version=3 \
    -cpu cortex-a76 \
    -smp 1 \
    -m 256 \
    -nographic \
    -kernel /tmp/rk3588_build/freertos_aarch64.elf
```

---

## 真机部署 (Orange Pi 5)

### 方法一：u-boot 直接加载

1. 将 `freertos_aarch64.bin` 复制到 SD 卡或 TFTP 服务器

2. 进入 u-boot 命令行（串口按 Ctrl+C 中断启动）

3. 加载并启动：
   ```
   # 从 SD 卡加载
   => load mmc 1:1 0x00200000 freertos_aarch64.bin

   # 或从 TFTP 加载
   => tftp 0x00200000 freertos_aarch64.bin

   # 释放 CPU4 (第一个 Cortex-A76 核心) 到地址 0x00200000
   => cpu 4 release 0x00200000
   ```

4. 通过调试串口查看输出：
   ```bash
   minicom -D /dev/ttyUSB0 -b 1500000
   ```

### 方法二：Linux remoteproc

```bash
# 将 bin 复制到 /lib/firmware
sudo cp freertos_aarch64.bin /lib/firmware/

# 通过 remoteproc 启动
echo freertos_aarch64.bin > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state
```

---

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

---

## 关键编译标志说明

| 标志 | 说明 |
|------|------|
| `-mcpu=cortex-a76` | 目标处理器 |
| `-mgeneral-regs-only` | 不使用浮点寄存器（简化上下文切换） |
| `-ffreestanding` | 裸金属环境，不依赖标准库 |
| `-nostdlib` | 不链接标准库 |
| `-include stdbool.h` | 提供 `bool` 类型支持 |
| `-DFREERTOS_AARCH64` | 启用 FreeRTOS AArch64 条件编译 |
| `-DSYS_CLOCK_HW_CYCLES_PER_SEC=1800000000` | Cortex-A76 主频 1.8GHz |
| `-DITERATIONS=10000` | 每项测试迭代次数 |

---

## 硬件地址映射 (Orange Pi 5 / RK3588)

| 外设 | 基地址 | 说明 |
|------|--------|------|
| UART2 | `0xFEB50000` | 调试串口 (16550 兼容) |
| GIC-600 Distributor | `0xFD000000` | GICv3 中断控制器 |
| GIC-600 Redistributor | `0xFD100000` | PPI 0-31 |
| ARM Generic Timer | 系统寄存器 | CNTPCT_EL0, CNTP_TVAL_EL0 |
| Timer EL1 Physical IRQ | 30 | GIC SPI 中断号 |
| FreeRTOS 加载地址 | `0x00200000` | 预留 16MB 内存区域 |

---

## 调试

### GDB 远程调试

```bash
# 终端 1：启动 JTAG 调试服务器
openocd -f interface/cmsis-dap.cfg -f target/rockchip_rk3588.cfg

# 终端 2：启动 GDB
aarch64-linux-gnu-gdb /tmp/rk3588_build/freertos_aarch64.elf
(gdb) target remote :3333
(gdb) break main
(gdb) continue
```

### 反汇编查看

```bash
aarch64-linux-gnu-objdump -d /tmp/rk3588_build/freertos_aarch64.elf | less
```

### 查看符号表

```bash
aarch64-linux-gnu-nm /tmp/rk3588_build/freertos_aarch64.elf | sort
```

---

## 故障排除

### 编译错误: `unknown type name 'bool'`

确保编译标志包含 `-include stdbool.h`。

### 编译错误: `unknown value 'cortex-a76' for '-mcpu'`

更新工具链版本。GCC 9+ 支持 Cortex-A76。

### 链接错误: `undefined reference to 'memset'`

确保 `bare_metal.c` 被编译并链接。

### 链接错误: `undefined reference to 'main'`

确保 `bench_all.c` 被编译并链接（它包含 `main()` 函数）。

### 真机无串口输出

1. 确认串口线连接正确（UART2, GPIO 引脚）
2. 确认波特率（默认 1500000）
3. 检查 `orange_pi_5.h` 中的 `UART2_BASE` 地址是否与实际板子匹配
4. 确认 u-boot 已正确将核心释放到 FreeRTOS 入口地址

### QEMU 无输出

正常现象。QEMU `virt` 机器使用 PL011 UART (0x9000000)，我们的代码使用 RK3588 的 16550 UART (0xFEB50000)。QEMU 仅用于验证编译和基本启动流程。

---

## 与标准 FreeRTOS AArch64 Port 的区别

| 方面 | 标准 ARM_AARCH64 port | 本项目自定义 port |
|------|----------------------|-------------------|
| GIC 访问 | 内存映射 (MMIO) | 系统寄存器 (ICC_*_EL1) |
| 执行级别 | EL3 | EL3 → EL1 降级 |
| 时钟源 | 外部 timer IP | ARM Generic Timer |
| UART | 板级特定 | 16550 兼容 |
| 上下文切换 | 标准 | GICv3 SGI/IRQ 感知 |
