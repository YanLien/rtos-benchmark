# FreeRTOS RTOS Benchmark - QEMU virt (Cortex-A55)

在 QEMU virt 机器上运行 FreeRTOS RTOS 性能基准测试。

## 前置条件

- `aarch64-linux-gnu-gcc` 交叉编译工具链
- `qemu-system-aarch64`
- [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) 源码

## 构建

```bash
# 指定 FreeRTOS-Kernel 路径
bash build.sh -k /path/to/FreeRTOS-Kernel

# 清理后重新构建
bash build.sh -k /path/to/FreeRTOS-Kernel -c

# 自定义输出目录
bash build.sh -k /path/to/FreeRTOS-Kernel -o /tmp/build
```

## 运行

```bash
# 构建并立即运行
bash build.sh -k /path/to/FreeRTOS-Kernel -r

# 或手动运行
qemu-system-aarch64 \
    -M virt,gic-version=3 \
    -cpu cortex-a55 \
    -smp 1 \
    -m 256 \
    -nographic \
    -kernel build/freertos_aarch64_qemu.elf
```

退出 QEMU: `Ctrl-A X`

## 配置

通过环境变量自定义测试参数:

```bash
ITERATIONS=1000 CALIBRATION_LOOPS=5000 bash build.sh -k /path/to/FreeRTOS-Kernel
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

- **UART**: PL011 (QEMU virt 默认)
- **中断控制器**: GICv3 (系统寄存器接口)
- **定时器**: ARM Generic Timer (EL1 Physical Timer)
- **时钟频率**: 62.5 MHz (CNTFRQ_EL0)
- **内存起始**: 0x40000000
- **运行模式**: EL1 裸金属 AMP
