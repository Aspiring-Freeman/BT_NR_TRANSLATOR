# FM33LG0xx Bootloader 指南 (Cortex-M0+)

## 问题说明

FM33LG0xx 使用 **Cortex-M0+** 内核，与 M3/M4 不同，M0+ **没有 VTOR (Vector Table Offset Register)** 寄存器。这意味着：

1. CPU 复位后总是从地址 **0x00000000** 读取向量表
2. 无法通过修改 VTOR 将向量表重定位到其他地址
3. FM33LG0xx 也没有像 STM32 那样的 SYSCFG->MEM_MODE 来重映射向量表到 RAM

## 内存布局

```
Flash 布局:
┌───────────────────────────────┐ 0x00000000
│  Bootloader 向量表 (192 bytes)│
├───────────────────────────────┤ 0x000000C0
│  Bootloader 代码              │
│  (约 16KB)                    │
├───────────────────────────────┤ 0x00004000  (APP_START_ADDRESS)
│  APP 向量表 (192 bytes)       │
├───────────────────────────────┤ 0x000040C0
│  APP 代码                     │
│  (约 240KB)                   │
└───────────────────────────────┘ 0x00040000
```

## 解决方案

### 方案 1：Bootloader 中断转发 (推荐)

**原理：** Bootloader 的向量表中每个中断处理程序都跳转到 APP 向量表中对应的处理程序。

**Bootloader 的中断处理程序实现：**

```c
#define APP_START_ADDRESS  0x00004000

// 通用中断转发宏
#define FORWARD_IRQ(irq_num) \
    __asm volatile( \
        "ldr r0, =0x00004000 + " #irq_num " * 4\n" \
        "ldr r0, [r0]\n" \
        "bx r0\n" \
    )

// 或者使用 C 函数：
void Forward_IRQ(uint32_t irq_index) {
    uint32_t *app_vector = (uint32_t *)APP_START_ADDRESS;
    void (*handler)(void) = (void (*)(void))app_vector[irq_index];
    handler();
}
```

**汇编实现 (更高效)：**

在 Bootloader 的 startup_fm33lg0xx.s 中，替换默认的中断处理程序：

```asm
// 中断转发模板 - 每个中断都跳转到 APP 的对应处理程序
.macro FORWARD_IRQ irq_offset
    ldr r0, =0x00004000 + \irq_offset
    ldr r0, [r0]
    bx r0
.endm

// SysTick 转发 (向量表偏移 15 * 4 = 60 = 0x3C)
SysTick_Handler:
    FORWARD_IRQ 0x3C

// UART0 转发 (向量表偏移 (16+10) * 4 = 104 = 0x68)
UART0_IRQHandler:
    FORWARD_IRQ 0x68
```

### 方案 2：RAM 向量表 (不适用于 FM33LG0xx)

某些 M0+ 芯片支持通过 SYSCFG 将地址 0x0 重映射到 SRAM，然后在 SRAM 开头放置向量表。但 **FM33LG0xx 不支持此功能**。

### 方案 3：无中断模式

如果 APP 不使用中断，可以简化实现：

1. Bootloader 跳转到 APP 前禁用所有中断
2. APP 只使用轮询方式

**不推荐**，因为大多数应用都需要中断。

## 实现步骤

### 1. 修改 Bootloader 启动文件

创建新的 `startup_fm33lg0xx_bootloader.s`：

```asm
// 向量表 - 中断转发到 APP
.section .isr_vector,"a",%progbits
.global g_pfnVectors
g_pfnVectors:
    .word _estack                // 初始栈指针
    .word Reset_Handler          // 复位处理程序 (Bootloader 自己的)
    .word NMI_Forward            // NMI -> 转发到 APP
    .word HardFault_Forward      // HardFault -> 转发到 APP
    // ... 其他中断也转发
```

### 2. APP 配置

APP 编译时使用 `fm33lg04x_app_with_bootloader.ld`，起始地址为 0x4000。

### 3. 烧录顺序

1. 先烧录 Bootloader 到 0x0000
2. 再烧录 APP 到 0x4000 (使用偏移烧录)

## 当前状态

当前 APP 项目 (`small_caliber_uwm_tester`) 已配置：

- `Inc/app_config.h` 中 `#define USE_BOOTLOADER` 控制模式
- CMakeLists.txt 根据配置选择链接脚本
- APP 起始地址为 0x4000

**Bootloader 项目** (`fm33lg0xx_bootloader`) 需要更新：

- 添加中断转发启动文件
- 确保 JumpToApp 正确设置 MSP 并跳转

## 调试检查清单

1. [ ] 确认 Bootloader 正确烧录到 0x0000
2. [ ] 确认 APP 正确烧录到 0x4000
3. [ ] 使用调试器检查 Bootloader 是否正确跳转
4. [ ] 检查 APP 的向量表第一个字 (MSP) 是否有效
5. [ ] 检查 APP 的向量表第二个字 (Reset_Handler) 地址是否正确
6. [ ] 如果使用中断，确认 Bootloader 实现了中断转发

## 参考资料

- ARM Cortex-M0+ Technical Reference Manual
- FM33LG0xx 数据手册
- AN4657: STM32 boot and startup (类似原理)
