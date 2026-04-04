# QEMU Cortex-A55 FreeRTOS Benchmark 卡住问题分析与修复

## 背景

在 QEMU `virt` 机器上使用 `cortex-a55` 启动 FreeRTOS benchmark 时，串口输出通常停在如下位置：

```text
FreeRTOS benchmark on QEMU virt (Cortex-A55)
[DEBUG] Initializing GIC...
[DEBUG] GIC initialized.
[DEBUG] Initializing timer...
[DEBUG] CNTFRQ_EL0 = 62500000
[DEBUG] Timer initialized.
Starting FreeRTOS scheduler...
!TASK!

 *** Starting! ***

** Thread stats [avg, min, max] in nanoseconds **
```

表面上看像是调度器启动后挂起，实际上是多个问题叠加导致在不同阶段失去前进能力。

## 现象

- 启动调度器后，任务切换会直接落入异常或死循环。
- `Thread stats` 打印后不再继续，误判为 benchmark 线程卡死。
- `Interrupt Stats` 阶段无法结束，导致最终看不到 `*** Done! ***`。
- 在 QEMU 上使用默认迭代次数时，即使没有逻辑死锁，也会因为运行时间过长而看起来像“挂住”。

## 根因分析

### 1. `yield` 路径不符合 AArch64 异常模型

原实现里 `portYIELD()` 直接调用 `vPortYield()`，随后在普通任务上下文里执行 `portSAVE_CONTEXT`。这条路径会去读取异常返回相关寄存器，前提是当前已经处于异常上下文。任务本身运行在 `EL1t`，不满足这个前提，因此在 QEMU A55 上会触发异常并掉进错误向量入口。

同时，向量表把 `Current EL with SP0` 的同步异常和 IRQ 都接到了 `hang`，一旦进入这条路径，系统就不会再恢复。

### 2. 任务退出路径存在死锁

`bench_thread_exit()` 会把待删除任务放入队列，然后立即阻塞在 `xSemaphoreTake(to_remove_sem, portMAX_DELAY)` 上。但这把信号量没有对应的释放者，结果是退出线程永久挂起，后续统计流程无法继续。

### 3. 上下文保存在共享异常栈上

早期实现把任务上下文压在共享的异常栈上，而不是当前任务自己的栈。这样做在第一次切换时可能还能工作，但多个任务切换后，先前任务保存的上下文会被后续异常覆盖，恢复时寄存器内容已经失真，最终表现为随机卡死、异常返回失败或任务不再调度。

### 4. 定时器 benchmark ISR 接线错误

中断延迟测试依赖于 benchmark 自己安装的定时器 ISR，但 IRQ 入口固定调用了 `FreeRTOS_Tick_Handler()`，没有转发到 benchmark 当前注册的处理函数。另外，`bench_timer_isr_expiry_set()` 返回的是相对周期数，而测试代码需要的是绝对触发时间戳；`bench_timer_cycles_diff()` 的差值方向也相反。这会让 `Interrupt Stats` 阶段的采样逻辑失效。

### 5. QEMU 默认负载过高，掩盖真实状态

QEMU 构建沿用了较大的 `ITERATIONS` 和 `CALIBRATION_LOOPS`。在真机上这些配置可以接受，但在模拟器里会显著拉长执行时间，即使程序没有死锁，也很容易被误认为“卡住”。

## 修复方案

### 1. 使用 `svc #0` 进入异常上下文再切换任务

- 将 `portYIELD()` 改为触发 `svc #0`。
- 新增 `FreeRTOS_SWI_Handler`，在异常上下文里完成 `portSAVE_CONTEXT`、`vTaskSwitchContext` 和 `portRESTORE_CONTEXT`。
- 调整异常向量表，把 `EL1 SP0` 和 `EL1 SPx` 的同步异常接到 FreeRTOS 的 SWI 处理入口，不再跳转到 `hang`。

涉及文件：

- `src/freertos_aarch64/port/portmacro.h`
- `src/freertos_aarch64/port/portASM.S`
- `src/freertos_aarch64/startup_aarch64.S`

### 2. 修正任务退出和资源回收流程

- `bench_thread_exit()` 不再等待一个无人释放的信号量。
- 退出线程改为先登记待删除句柄，再 `give` 信号量通知回收线程，最后 `vTaskSuspend(NULL)`。
- `bench_collect_resources()` 在回收前消费这把信号量，确保删除流程成对出现。

涉及文件：

- `src/freertos_aarch64/bench_porting_layer_aarch64.c`

### 3. 按 FreeRTOS AArch64 约定把上下文保存在任务栈中

- `portSAVE_CONTEXT` 改为切到 `SP_EL0`，把寄存器现场压到当前任务栈。
- 当前任务栈顶写回 TCB，恢复时从 TCB 取回。
- `pxPortInitialiseStack()` 的初始栈布局同步调整为和汇编保存布局一致。

这样每个任务都有独立的上下文保存区，不会再互相覆盖。

涉及文件：

- `src/freertos_aarch64/port/portASM.S`
- `src/freertos_aarch64/port/port.c`

### 4. 修正定时器 benchmark 处理链

- 默认定时器 ISR 改为一个包装函数，由它调用 `FreeRTOS_Tick_Handler()`。
- IRQ 入口针对定时器中断时，不再硬编码调用 tick handler，而是调用当前注册的 benchmark ISR。
- `bench_timer_isr_expiry_set()` 改为返回绝对触发时间戳。
- `bench_timer_cycles_diff()` 改为按 `sample - trigger` 计算。
- 恢复 ISR 时若传入空指针，回退到 FreeRTOS tick 包装函数。

涉及文件：

- `src/freertos_aarch64/timer/bench_generic_timer.c`
- `src/freertos_aarch64/bench_porting_layer_aarch64.c`

### 5. 禁用不必要的 QEMU FPU 上下文保存

QEMU 当前启动路径下并没有使用到 benchmark 所需的 FP/SIMD 计算，但原配置要求总是保存 FPU 上下文。第一次执行到 `stp q0, q1` 时会触发异常。为避免这个问题，配置改为按需启用任务 FPU 支持。

涉及文件：

- `src/freertos_aarch64/FreeRTOSConfig.h`

### 6. 为 QEMU 单独降低默认迭代规模

- QEMU 构建脚本默认把 `ITERATIONS` 调整为 `100`。
- QEMU 构建脚本默认把 `CALIBRATION_LOOPS` 调整为 `1000`。
- QEMU 专用 CMake 配置也会在未显式覆盖时使用这组较小默认值。

这样可以更快区分“真的死锁”和“只是模拟器执行慢”。

涉及文件：

- `scripts/build_freertos_aarch64_qemu.sh`
- `src/freertos_aarch64/freertos_aarch64_qemu.cmake`

## 验证方法

### 重新编译

```bash
bash scripts/build_freertos_aarch64_qemu.sh -c
```

### 在 QEMU 中运行

```bash
qemu-system-aarch64 \
    -M virt,gic-version=3 \
    -cpu cortex-a55 \
    -smp 1 \
    -m 256 \
    -nographic \
    -kernel build/qemu_virt/freertos_aarch64_qemu.elf
```

### 预期结果

程序不应停留在 `Thread stats` 或 `Interrupt Stats` 标题处，而应继续输出完整统计并最终打印：

```text
*** Done! ***
```

## 建议

- 在 QEMU 上优先使用较小的 `ITERATIONS` 和 `CALIBRATION_LOOPS`，确认功能正确后再逐步提高。
- 如果后续再次遇到异常停机，建议在同步异常入口额外打印 `ESR_EL1`、`ELR_EL1` 和 `FAR_EL1`，可以更快区分是 `SVC`、未定义指令还是数据访问异常。
- 若后续需要启用任务级 FP/SIMD 计算，再单独补齐 `CPACR_EL1` 初始化和对应的上下文保存策略，不要只修改 `configUSE_TASK_FPU_SUPPORT`。

## 当前结果

经过上述修复后，QEMU `virt + cortex-a55` 下的 FreeRTOS benchmark 已能够从调度器启动一路运行到结束，完整输出线程统计、中断统计，并以 `*** Done! ***` 收尾。
