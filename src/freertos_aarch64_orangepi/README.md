# FreeRTOS RTOS Benchmark - Orange Pi 5 Plus (RK3588)

在 Orange Pi 5 Plus (RK3588) 开发板上运行 FreeRTOS RTOS 性能基准测试。

以裸金属 AMP 模式在 Cortex-A55 小核上运行，与 Linux 系统并行。

## 前置条件

- `aarch64-linux-gnu-gcc` 交叉编译工具链
- [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) 源码
- Orange Pi 5 Plus 开发板，已启动 u-boot

## 构建

```bash
# 指定 FreeRTOS-Kernel 路径
bash build.sh -k /path/to/FreeRTOS-Kernel

# 清理后重新构建
bash build.sh -k /path/to/FreeRTOS-Kernel -c

# 自定义输出目录
bash build.sh -k /path/to/FreeRTOS-Kernel -o /tmp/build
```

## 部署到 Orange Pi

### 方法一: 使用 build.sh 的 -f 参数

```bash
bash build.sh -k /path/to/FreeRTOS-Kernel -f <orangepi-ip>
```

### 方法二: 手动部署

```bash
# 通过 scp 传输二进制文件
scp build/freertos_aarch64_orangepi.bin root@<orangepi-ip>:/tmp/

# 或通过 SD 卡 / USB
cp build/freertos_aarch64_orangepi.bin /path/to/sd-card/
```

## 在 u-boot 中启动

在 Orange Pi 的 u-boot 控制台中:

```bash
# 从 SD 卡加载二进制文件到内存
=> fatload mmc 1:1 0x40000000 freertos_aarch64_orangepi.bin

# 启动执行
=> go 0x40000800
```

通过串口 (UART2, 1500000 波特率) 查看输出结果。

## 配置

通过环境变量自定义测试参数:

```bash
ITERATIONS=5000 CALIBRATION_LOOPS=5000 bash build.sh -k /path/to/FreeRTOS-Kernel
```

## 测试项目

- `thread` - 线程创建/销毁
- `malloc_free` - 内存分配/释放
- `message_queue` - 消息队列
- `mutex_lock_unlock` - 互斥锁加锁/解锁
- `sem_context_switch` - 信号量上下文切换
- `sem_signal_release` - 信号量信号释放
- `thread_switch_yield` - 线程切换/让步
- `interrupt_latency` - 中断延迟

## 架构说明

- **UART**: 16550 (RK3588 UART2, 波特率 1500000)
- **中断控制器**: GIC-600 (GICv3 系统寄存器接口)
- **定时器**: ARM Generic Timer (EL1 Physical Timer, 24 MHz)
- **CPU 时钟**: 1.8 GHz (Cortex-A55 LITTLE 集群)
- **内存起始**: 0x40000000 (保留 RAM 区域, 16MB)
- **运行模式**: EL1 裸金属 AMP (与 Linux 并行运行)
- **目标核心**: Cortex-A55 (CPU0-3 LITTLE 集群)
