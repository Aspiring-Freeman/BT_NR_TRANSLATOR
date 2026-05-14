# FM33LG0xx Bootloader 文件

本目录包含 Bootloader 项目所需的核心文件。

## 文件说明

| 文件 | 说明 |
|------|------|
| `startup_fm33lg0xx_bootloader.s` | **启动文件** - 实现中断转发到 APP |
| `bootloader_main.c` | **主程序** - 检查 APP 有效性并跳转 |
| `fm33lg04x_bootloader.ld` | **链接脚本** - Bootloader 内存布局 (16KB) |

## 使用方法

### 1. 复制文件到 Bootloader 项目

```bash
# 假设 Bootloader 项目在 ../fm33lg0xx_bootloader
cp startup_fm33lg0xx_bootloader.s ../fm33lg0xx_bootloader/Startup/
cp bootloader_main.c ../fm33lg0xx_bootloader/Src/
cp fm33lg04x_bootloader.ld ../fm33lg0xx_bootloader/VscodeGcc/
```

### 2. 修改 Bootloader 项目的 CMakeLists.txt

```cmake
# 使用新的启动文件
set(ASM_SOURCES
    Startup/startup_fm33lg0xx_bootloader.s
)

# 使用 Bootloader 链接脚本
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/VscodeGcc/fm33lg04x_bootloader.ld)
```

### 3. 编译 Bootloader

```bash
cd ../fm33lg0xx_bootloader
cmake -S . -B build
cmake --build build
```

### 4. 烧录顺序

**先烧录 Bootloader:**
```bash
pyocd flash build/Bootloader.elf --target fm33lg04x
```

**再烧录 APP (带偏移):**
```bash
# APP 使用 Bootloader 模式编译 (USE_BOOTLOADER=ON)
pyocd flash build/SmallCaliberUWMTester.elf --target fm33lg04x
```

## 工作原理

### 中断转发机制

由于 Cortex-M0+ 没有 VTOR 寄存器，无法修改向量表基地址。因此：

1. **Bootloader 的向量表** (0x0000) 包含：
   - 前两项是 Bootloader 自己的 (SP, Reset_Handler)
   - 其他所有中断处理程序都是转发函数

2. **转发函数** 读取 APP 向量表 (0x4000) 对应位置的地址并跳转：
   ```asm
   UART0_IRQForward:
       ldr   r0, =0x00004000 + 0x68  ; UART0 在向量表中的偏移
       ldr   r0, [r0]                 ; 读取 APP 的 UART0_IRQHandler 地址
       bx    r0                       ; 跳转执行
   ```

3. **当中断发生时：**
   - CPU 从地址 0x0000 的向量表读取处理程序地址
   - 执行 Bootloader 的转发函数
   - 转发函数跳转到 APP 的实际处理程序

### 内存布局

```
Flash (256KB):
┌─────────────────────────────┐ 0x00000000
│ Bootloader 向量表 (192B)    │
│ - SP, Reset: Bootloader 自己│
│ - 其他: 转发到 APP          │
├─────────────────────────────┤ 0x000000C0
│ Bootloader 代码 (~16KB)     │
├─────────────────────────────┤ 0x00004000
│ APP 向量表 (192B)           │
├─────────────────────────────┤ 0x000040C0
│ APP 代码 (~240KB)           │
└─────────────────────────────┘ 0x00040000

RAM (32KB):
┌─────────────────────────────┐ 0x20000000
│ 共享 RAM                    │
│ (Bootloader 和 APP 都使用)  │
└─────────────────────────────┘ 0x20008000
```

## 注意事项

1. **APP 配置**
   - 确保 `app_config.h` 中启用 `USE_BOOTLOADER`
   - APP 起始地址必须是 0x4000

2. **中断延迟**
   - 中断转发会增加约 3-5 个时钟周期的延迟
   - 对于大多数应用来说可以忽略

3. **调试**
   - 调试 APP 时，确保 Bootloader 已正确烧录
   - 可以在 APP 的 Reset_Handler 设置断点验证跳转是否成功

## 故障排除

### APP 不执行

1. 检查 Bootloader 是否正确烧录到 0x0000
2. 检查 APP 是否正确烧录到 0x4000
3. 检查 APP 的向量表是否有效 (SP 和 Reset_Handler)

### 中断不工作

1. 确认使用的是中断转发版本的启动文件
2. 检查中断号是否正确 (FM33LG0xx 特定)
3. 用调试器跟踪中断转发过程

### 程序死机

1. 可能是 HardFault - 检查 APP 的栈是否溢出
2. 检查 APP 是否正确初始化了中断向量
