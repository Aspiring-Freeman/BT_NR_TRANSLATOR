# ELF Program Header Flash Address Issue

## 问题描述 (Problem Description)

在Debian 13系统上使用PyOCD 0.36.0烧录ELF文件时，APP被错误地写入到0x0地址而不是0x4000地址，导致Bootloader被覆盖。但在Debian 12系统上使用相同的ELF文件没有问题。

When flashing ELF files with PyOCD 0.36.0 on Debian 13, the APP was incorrectly written to address 0x0 instead of 0x4000, overwriting the bootloader. However, the same ELF file worked correctly on Debian 12.

## 根本原因 (Root Cause)

**GNU LD linker的Program Header地址设置问题**

即使linker script中设置了`MEMORY { FLASH (rx) : ORIGIN = 0x4000, LENGTH = 240K }`，GNU LD创建的ELF Program Header中的`PhysAddr`字段仍然是`0x0`，而不是`0x4000`。

PyOCD读取ELF的**Program Headers**来决定烧录地址，而不是读取**Section Headers**。

Even though the linker script specified `MEMORY { FLASH (rx) : ORIGIN = 0x4000, LENGTH = 240K }`, GNU LD created ELF Program Headers with `PhysAddr` field set to `0x0` instead of `0x4000`.

PyOCD reads ELF **Program Headers** to determine flash addresses, not **Section Headers**.

---

## ELF文件结构详解 (ELF File Structure Explained)

### 1. ELF文件包含两种地址信息

ELF文件同时包含两种不同的地址映射：

- **Section Headers**: 用于链接和调试，包含VMA (Virtual Memory Address) 和 LMA (Load Memory Address)
- **Program Headers**: 用于程序加载（运行时和烧录），包含VirtAddr和PhysAddr

**Section Headers**: Used for linking and debugging, contains VMA and LMA
**Program Headers**: Used for program loading (runtime and flashing), contains VirtAddr and PhysAddr

### 2. 为什么有两种Header？

- **Section Headers**: 编译器/链接器/调试器使用，细粒度（每个section一个header）
- **Program Headers**: 操作系统/bootloader/烧录工具使用，粗粒度（多个section合并到一个segment）

Sections are fine-grained (one per .text, .data, .bss, etc.), while segments are coarse-grained (multiple sections grouped into loadable segments).

### 3. PyOCD为什么读Program Headers？

PyOCD模拟的是一个"加载器"的角色，它要把程序加载到MCU的Flash中。在标准的ELF加载过程中：

1. **程序加载器只读取Program Headers** (PT_LOAD segments)
2. 每个PT_LOAD segment指定了：
   - `PhysAddr`: 物理地址（Flash中的地址）
   - `VirtAddr`: 虚拟地址（运行时的地址）
   - `FileSiz`: 文件中的大小
   - `MemSiz`: 内存中的大小

PyOCD follows this standard ELF loading convention and only reads Program Headers (PT_LOAD segments) to determine where to flash the data.

---

## 如何检查ELF文件 (How to Inspect ELF Files)

### 查看Program Headers

```bash
arm-none-eabi-readelf -l SmallCaliberUWMTester.elf
```

**输出示例 (Output Example):**

```
Program Headers:
  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD           0x010000 0x00004000 0x00004000 0x138d4 0x138d4 R E 0x10000  ← 正确！PhysAddr = 0x4000
  LOAD           0x0238d4 0x200000d8 0x000178d4 0x000dc 0x000dc RW  0x10000  ← .data段
  LOAD           0x000000 0x200001b4 0x200001b4 0x00000 0x01cdc RW  0x1      ← .bss段
```

**问题表现 (Problem Pattern):**
```
  LOAD           0x010000 0x00004000 0x00000000 0x138d4 0x138d4 R E 0x10000  ← 错误！PhysAddr = 0x0
```

### 查看Section Headers

```bash
arm-none-eabi-objdump -h SmallCaliberUWMTester.elf
```

**输出示例 (Output Example):**

```
Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .isr_vector   000000d8  00004000  00004000  00010000  2**2
  1 .text         000137fc  000040d8  000040d8  000100d8  2**2
  2 .rodata       00000000  000178d4  000178d4  000238d4  2**0
  3 .data         000000dc  200000d8  000178d4  000238d4  2**2
  4 .bss          00001cdc  200001b4  000179b0  00000000  2**2
```

注意：Section Headers的LMA是正确的(0x00004000)，但PyOCD不读这个！

Note: Section Headers have correct LMA (0x00004000), but PyOCD doesn't read this!

### 完整的ELF信息查看

```bash
# 查看所有ELF header信息
arm-none-eabi-readelf -a SmallCaliberUWMTester.elf

# 只查看segments到sections的映射
arm-none-eabi-readelf -l SmallCaliberUWMTester.elf | grep -A 20 "Segment Sections"
```

---

## HEX vs ELF vs BIN (File Format Comparison)

### Intel HEX (.hex)

**结构**: ASCII文本格式，每行包含地址和数据

```
:020000040000FA          ← Extended Linear Address (0x0000)
:1000000000800020FD02000049030000...
:020000040004F6          ← Extended Linear Address (0x4000)
:10400000000080206D6E000073940000...
```

**地址信息**: 
- 明确包含绝对地址
- 每个数据记录都有地址前缀
- **PyOCD直接读取HEX中的地址字段** → 正确烧录到0x4000

**优点**: 地址信息明确，不依赖文件格式解析
**缺点**: 文件较大，ASCII格式，不包含调试信息

### ELF (.elf)

**结构**: 二进制格式，包含完整的程序、调试信息、符号表

**地址信息**:
- Section Headers (VMA/LMA) - 用于链接和调试
- Program Headers (VirtAddr/PhysAddr) - 用于加载

**PyOCD的处理**:
- PyOCD 0.36.0读取Program Headers的PhysAddr字段
- 如果PhysAddr = 0x0 → 烧录到0x0 ✗
- 如果PhysAddr = 0x4000 → 烧录到0x4000 ✓

**优点**: 包含调试信息，GDB可以使用符号表，调试时可以看到变量名、函数名
**缺点**: 依赖Program Headers设置正确

### BIN (.bin)

**结构**: 纯二进制数据，没有任何地址信息

**地址信息**:
- 无！BIN文件不包含地址信息
- 必须手动指定烧录地址

**PyOCD的处理**:
```bash
pyocd flash firmware.bin --base-address 0x4000  # 必须手动指定地址
```

**优点**: 文件最小，格式简单
**缺点**: 
- 必须手动指定地址
- 没有调试信息
- GDB调试时只能看到地址，看不到符号

### 格式对比总结

| 格式 | 地址信息 | 调试信息 | PyOCD处理 | 适用场景 |
|------|----------|----------|-----------|----------|
| HEX  | 明确包含 | 无 | 直接读取地址 | 生产烧录 |
| ELF  | Program Headers | 完整 | 读取PhysAddr | 开发调试 |
| BIN  | 无 | 无 | 需手动指定 | 批量生产 |

---

## PHDRS指令详解 (PHDRS Directive Explained)

### 什么是PHDRS？

`PHDRS`是GNU LD linker script中的一个指令，用于**显式控制ELF Program Headers的创建**。

PHDRS is a GNU LD linker script directive used to **explicitly control ELF Program Header creation**.

### 为什么需要PHDRS？

默认情况下，GNU LD会自动创建Program Headers，但是：

1. **自动创建的Program Headers可能不正确**
   - 即使`MEMORY`设置了`ORIGIN = 0x4000`
   - GNU LD可能仍然设置`PhysAddr = 0x0`

2. **PHDRS提供显式控制**
   - 通过`:segment_name`语法将sections分配到特定的segment
   - 确保每个segment的PhysAddr正确设置

By default, GNU LD auto-creates Program Headers, but they may have incorrect PhysAddr even when `MEMORY` specifies `ORIGIN = 0x4000`.

### PHDRS语法

```ld
PHDRS
{
  segment_name type [FILEHDR] [PHDRS] [AT(address)] [FLAGS(flags)];
}
```

**常用的type类型**:
- `PT_LOAD`: 可加载的段（代码、数据）
- `PT_NULL`: 空段
- `PT_DYNAMIC`: 动态链接信息

### 我们的解决方案

```ld
PHDRS
{
  flash PT_LOAD;      /* 代码段和只读数据，烧录到Flash */
  ram_init PT_LOAD;   /* .data段，初始值在Flash，运行时复制到RAM */
  ram PT_LOAD;        /* .bss段，只在RAM中，不占用Flash */
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH :flash       /* ← 分配到flash segment */

  .text :
  {
    /* ... */
  } >FLASH :flash       /* ← 分配到flash segment */

  .rodata :
  {
    /* ... */
  } >FLASH :flash       /* ← 分配到flash segment */

  .data : 
  {
    /* ... */
  } >RAM AT> FLASH :ram_init    /* ← 分配到ram_init segment */

  .bss :
  {
    /* ... */
  } >RAM :ram           /* ← 分配到ram segment */
}
```

### PHDRS的工作原理

1. **定义3个segments**:
   - `flash`: 包含.isr_vector, .text, .rodata
   - `ram_init`: 包含.data (LMA在Flash, VMA在RAM)
   - `ram`: 包含.bss (只在RAM)

2. **GNU LD的处理**:
   ```
   flash segment: PhysAddr = FLASH的ORIGIN = 0x4000 ✓
   ram_init segment: PhysAddr = .data的LMA (在Flash中)
   ram segment: PhysAddr = .bss的VMA (在RAM中)
   ```

3. **结果**:
   ```
   Program Headers:
     Type   Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg
     LOAD   0x010000 0x00004000 0x00004000 0x138d4 0x138d4 R E  ← PhysAddr = 0x4000 ✓
   ```

---

## 调试工具总结 (Debugging Tools Summary)

### 1. arm-none-eabi-readelf

**最重要的命令**：

```bash
# 查看Program Headers (PyOCD读这个!)
arm-none-eabi-readelf -l firmware.elf

# 查看Section Headers
arm-none-eabi-readelf -S firmware.elf

# 查看所有信息
arm-none-eabi-readelf -a firmware.elf
```

### 2. arm-none-eabi-objdump

```bash
# 查看Section Headers和地址
arm-none-eabi-objdump -h firmware.elf

# 反汇编查看代码
arm-none-eabi-objdump -d firmware.elf

# 查看完整内容
arm-none-eabi-objdump -x firmware.elf
```

### 3. arm-none-eabi-objcopy

```bash
# ELF转HEX
arm-none-eabi-objcopy -O ihex firmware.elf firmware.hex

# ELF转BIN
arm-none-eabi-objcopy -O binary firmware.elf firmware.bin

# 查看支持的格式
arm-none-eabi-objcopy --help
```

### 4. PyOCD验证

```bash
# 烧录后读取Flash验证
pyocd commander -t fm33lg04x --pack xxx.pack -c "read32 0x4000 16"

# 应该看到ARM Cortex-M vector table:
# 第一个word: Stack pointer (例如 0x20008000)
# 第二个word: Reset handler (例如 0x00006e6d)
```

---

## 为什么Debian 12可以但Debian 13不行？

可能的原因：

1. **arm-none-eabi-gcc版本差异**
   - Debian 12: 可能使用较老版本的GCC
   - Debian 13: arm-none-eabi-gcc 14.2.1
   - 不同版本的LD行为可能不同

2. **PyOCD版本差异**
   - 可能Debian 12使用的PyOCD版本更老，处理逻辑不同
   - PyOCD 0.36.0在Debian 13上严格按Program Headers处理

3. **Binutils版本**
   - GNU LD (linker)的行为在不同版本可能有变化

**解决方案**: 使用PHDRS显式控制，确保跨版本兼容性

---

## 最佳实践 (Best Practices)

### 1. 使用PHDRS显式控制

对于bootloader + app的项目，**始终使用PHDRS指令**：

```ld
PHDRS
{
  flash PT_LOAD;
  ram_init PT_LOAD;
  ram PT_LOAD;
}
```

### 2. 编译后验证

每次修改linker script后，检查Program Headers：

```bash
arm-none-eabi-readelf -l build/firmware.elf | grep "LOAD"
```

确认PhysAddr字段正确。

### 3. 三种格式的使用场景

- **开发阶段**: 使用ELF + GDB调试（有符号表）
- **生产烧录**: 使用HEX（地址明确，兼容性好）
- **批量生产**: 使用BIN（文件最小）

### 4. PyOCD烧录验证

烧录后读取关键地址验证：

```bash
# 读取0x0 (Bootloader)
pyocd commander -c "read32 0x0 4"
# 应该看到: 20008000 000002fd (stack pointer + reset vector)

# 读取0x4000 (APP)  
pyocd commander -c "read32 0x4000 4"
# 应该看到: 20008000 00006e6d (stack pointer + reset vector)
```

---

## 参考资料 (References)

1. **GNU LD Manual - PHDRS Command**
   - https://sourceware.org/binutils/docs/ld/PHDRS.html

2. **ELF Format Specification**
   - https://refspecs.linuxfoundation.org/elf/elf.pdf

3. **ARM Cortex-M Programming**
   - ARM Cortex-M0+ Generic User Guide

4. **PyOCD Documentation**
   - https://pyocd.io/docs/

---

## 相关文件 (Related Files)

- Linker Script: `Drivers/CMSIS/Device/FM/FM33xx/Source/Templates/gcc/linker/fm33lg04x_app_with_bootloader.ld`
- VSCode Tasks: `.vscode/tasks.json`
- Version Info: `VERSION.md`

---

*文档创建时间: 2026-01-07*
*问题解决者: GitHub Copilot*
*MCU: FM33LG04x (ARM Cortex-M0+)*
