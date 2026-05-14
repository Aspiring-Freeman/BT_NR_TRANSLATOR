# MeterUart 初始化后 PC UART (UART1) RX 完全失效

> 关键词：FM33LG04x、UART1、LPUART1、GPIO 复用、`remapPin`、DFS、引脚 mux 冲突

## 1. 现象

- 单独跑 `PCUart_Init()`（UART1 / PB13 / PB14）：上位机能正常收发，RX 中断触发正常。
- 在 `Peripheral_Init()` 里依次调用 `PCUart_Init()` → `MeterUart_Init()` 之后：
  - UART1 寄存器看起来"完全正常"——`IER.RXBFIE = 1`、`CSR` 配置正确、NVIC 已使能。
  - 但 PC 发数据过来，UART1 **永远不进 RX 中断**，轮询 `ISR.RXBF` 也始终为 0。
- bisection 证据：注释掉 `MeterUart_Init()` → RX 立刻恢复；其它外设（UART3 / UART4 / ATIM / ADC...）全开都不影响。

## 2. 直接原因

`MeterUart_GPIO_Init()` 把 `PC2 / PC3` 配成数字复用主功能 + `remapPin = FL_DISABLE`，导致 **PC2/PC3 被路由到 UART1 而不是 LPUART1**，与 PCUart 在 PB13/PB14 上的 UART1 抢同一组外设输入。

## 3. 根本原因（硬件层面）

### 3.1 FM33LG04x 的引脚复用规则

查 KiCad 原理图符号库 [docs/5、项目文件/kicad_pcb_document/small_umw_board/small_umw_board-altium-import.kicad_sym](../5、项目文件/kicad_pcb_document/small_umw_board/small_umw_board-altium-import.kicad_sym) 可以确认：

| 引脚 | 主功能（`remapPin = FL_DISABLE`，DFS=0） | 重映射（`remapPin = FL_ENABLE`，DFS=1） |
|------|------|------|
| PB13 | **UART1_RX** | LPUART1_RX |
| PB14 | **UART1_TX** | LPUART1_TX |
| PC2  | **UART1_RX** | LPUART1_RX |
| PC3  | **UART1_TX** | LPUART1_TX |

PB13/PB14 和 PC2/PC3 在主功能层是**同一组外设的镜像**，都连到 UART1。要让 PC2/PC3 接到 LPUART1 必须额外置 GPIO **DFS（Digital Function Select）= 1**，也就是 FL 库里的 `FL_GPIO_EnablePinRemap()`。

### 3.2 MeterUart 写错在哪里

[Inc/Peripheral/uart/meter_uart.h](../../Inc/Peripheral/uart/meter_uart.h) 里的宏定义看起来**完全正确**：
```c
#define METER_UART_INSTANCE     LPUART1          // 外设实例选 LPUART1
#define METER_UART_IRQn         LPUARTx_IRQn     // 中断也是 LPUART
#define METER_FL_Init           FL_LPUART_Init   // 调 FL_LPUART_Init
#define METER_UART_GPIO_PORT    GPIOC
#define METER_UART_RX_PIN       FL_GPIO_PIN_2
#define METER_UART_TX_PIN       FL_GPIO_PIN_3
```
所以 LPUART1 外设本身（CR / CSR / IER / 中断向量）确实被正确初始化了。

但是 [Src/Peripheral/uart/meter_uart.c](../../Src/Peripheral/uart/meter_uart.c) 的 `MeterUart_GPIO_Init()` 漏了关键一步：
```c
GPIO_InitStruct.remapPin = FL_DISABLE;   // ← BUG：PC2/PC3 主功能 = UART1
FL_GPIO_Init(METER_UART_GPIO_PORT, &GPIO_InitStruct);
```
结果：

- LPUART1 外设里 baud / IER 都设好了，但**没有任何 GPIO 把信号送到 LPUART1**——所以 LPUART1 永远收不到东西（同时这也意味着 LPUART1 那条通道一直没在工作，没人发现）。
- 真正出现在 PC2 引脚上的电平被路由到了 **UART1_RX**——也就是 PCUart 正在用的那个外设。

### 3.3 为什么 PCUart 自己没"覆盖"掉 MeterUart？

这是一个常见的认知误区。`PCUart_Init` 和 `MeterUart_Init` 写的是**两组完全不同的 GPIO 寄存器**：

- `PCUart_Init` → 写 `GPIOB` 的 PB13/PB14
- `MeterUart_Init` → 写 `GPIOC` 的 PC2/PC3

两边在 GPIO 层面互不接触，所以谁都不会被对方"覆盖"。问题不在 GPIO 层，在它**下游的外设输入选择器**上。

### 3.4 为什么 RX 会"完全死"——UART1_RX 输入 mux 被同时驱动

FM33LG04x 的 UART1_RX 在芯片内部只有**一条输入信号线**，但同时被 PB13 和 PC2 两个 GPIO 通过主功能 mux 喂入。当：

- PB13：连到 PC 的 TX，闲时高电平，发数据时按串行码型跳变。
- PC2：连接外部表具回路，要么悬空，要么接到与外设 idle 不一致的电平（外部上下拉、缓冲器输出、悬空噪声都有可能）。

这种简单 mux 在 FMSH 这类小 MCU 上多数实现为**线与 / wired-AND**（idle 高电平为正常 UART 约定）：

- 若 PC2 长期为低 → UART1_RX 永远看到 0，停止位拼不出，永远没有完整字节，`RXBF` 永远不置位。
- 若 PC2 信号乱跳 → 起始位 / 数据位 / 停止位会被另一路毛刺撕裂 → 帧错误，但即使有错也不一定置 `RXBF`，看起来就是"全聋"。

发送方向同理（PB14 + PC3 同时被 UART1_TX 驱动），但因为两边都是"输出"，方向冲突表现没那么直观，也不是这次故障的主线。

### 3.5 为什么寄存器看着完全正常

bisection 时打印的 `UART1.ISR=0x00000003 INEN=0x0000C000`：

- `ISR=0x03` = `TXSE | TXBE`，这是**发送侧 idle 状态正常**，不能说明接收侧。
- `INEN=0xC000` = PB14 | PB13 输入使能 OK。
- 所有 UART1 自身寄存器都对——因为问题不在 UART1 自己，而在喂给它的物理信号已经被 PC2 污染了。这就是为什么之前在 UART1 寄存器里查了好几轮都查不到。

这也解释了为什么前面那个 INEN 假线索（"`FL_GPIO_Init` DIGITAL 模式会调 `DisablePinInput`"）能挑战那么久——它确实是 FL 库的一个真坑，但**不是这次故障的因**。

## 4. 修复

修改两个文件：

[Inc/Peripheral/uart/meter_uart.h](../../Inc/Peripheral/uart/meter_uart.h) 增加自动选择 remap 的宏：
```c
#if METER_UART_USE_LPUART
#define METER_UART_GPIO_REMAP FL_ENABLE   // 走 LPUART1，必须 remap
#else
#define METER_UART_GPIO_REMAP FL_DISABLE  // 走 UART1，主功能即可
#endif
```

[Src/Peripheral/uart/meter_uart.c](../../Src/Peripheral/uart/meter_uart.c) 在 RX/TX 两次 `FL_GPIO_Init` 前都用这个宏：
```c
GPIO_InitStruct.remapPin = METER_UART_GPIO_REMAP;
```

效果：PC2/PC3 退出 UART1 的 mux 竞争，UART1_RX 的输入线只剩 PB13 一个驱动者，PCUart 立刻恢复正常；同时 LPUART1 也终于真正有 GPIO 喂数据了，MeterUart 也才真正开始工作。

## 5. 经验教训

1. **复用 mux 的"主 / 重映射"不是 GPIO 的属性，而是外设的物理通路**。配错 GPIO 的 remap，伤害的是**别的外设**而不是自己——故障表象会出现在"无辜"模块身上，极难定位。
2. **看 ARM 芯片"引脚复用表"时要看完整**：当一个引脚的主功能和另一个引脚冲突在同一个外设上，必须当作"两个引脚共享一条内部线"来理解，绝不能并存。
3. **当一个模块的所有自身寄存器都正常但功能全失时，要怀疑物理输入被污染**，而不是继续盯着自己的寄存器看。这次浪费了大量时间在 UART1 的 IER / INEN 上。
4. **抽象（`METER_UART_INSTANCE = LPUART1`）能骗过头脑**：宏看起来都对，但 GPIO 那一行 `remapPin = FL_DISABLE` 把抽象层撕了个洞。涉及硬件复用的封装，必须把"是否 remap"作为**强制参数**而不是默认值。
5. **bisection 是这种"幽灵故障"最可靠的工具**。寄存器分析容易自我欺骗，二分法不会撒谎。
