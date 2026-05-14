# Issue: fal_def.h 劫持 EasyLogger 日志宏导致 Keil 构建 HardFault

- **日期**: 2026-02-26
- **影响**: Keil 构建的固件在无调试器运行时崩溃（HardFault），GCC 构建不受影响
- **状态**: ✅ 已修复
- **严重性**: 致命 — 系统启动后约 2~3 秒内崩溃

---

## 1. 现象描述

### Keil 构建表现
烧录 Keil 编译的固件后，系统启动日志输出到 `Components_Init()` 的测试日志后，
后续所有 `log_i` 输出**内容全部相同**（重复输出同一条旧日志），约 3.2 秒后系统崩溃：

```
RX ← I/components [00:01.803] 随便测试下...说明组件初始化函数被调用了
RX ← I/components [00:01.803] 随便测试下...说明组件初始化函数被调用了  ← 重复！
RX ← I/components [00:01.803] 随便测试下...说明组件初始化函数被调用了  ← 重复！
RX ← I/components [00:01.803] 随便测试下...说明组件初始化函数被调用了  ← 重复！
...（共 7 次重复后崩溃）
```

注意：
- TAG 始终是 `I/components`，但后续代码的 TAG 应该是 `I/main`
- 时间戳不变，说明内容是旧的缓冲区残留

### GCC/VSCode 构建表现
完全正常，日志输出正确，系统稳定运行。

---

## 2. 根因分析

### 2.1 核心问题：`fal_def.h` 劫持了 `log_i` / `log_e` / `log_d` 宏

在 `components.h` 中，头文件包含顺序如下：

```c
// components.h 中的包含顺序

#ifdef COMPONENT_EASYLOGGER
#include "EasyLogger/easylogger/inc/elog.h"   // ① 定义 log_i → elog_output()
#endif

#ifdef COMPONENT_FLASHDB
#include "FlashDB/port/fal/inc/fal.h"         // ② fal.h → fal_def.h
#endif
```

`fal_def.h`（FlashDB/FAL 库的定义头文件）中有如下代码：

```c
// fal_def.h (第三方库代码)

#ifndef FAL_PRINTF
#define FAL_PRINTF  printf    // 默认使用 printf
#endif

#ifdef  log_i
#undef  log_i                 // ← 强制取消 EasyLogger 的定义！
#endif
#define log_i(...)  FAL_PRINTF("\033[32;22m[I/FAL] "); FAL_PRINTF(__VA_ARGS__); FAL_PRINTF("\033[0m\n")

// 同样处理 log_e、log_d
```

**结果**：所有 `#include "components.h"` 之后的源文件（如 `main.c`、`uart0.c` 等）中，
`log_i(...)` 被展开为 `printf(...)` 而非 `elog_output(...)`。

### 2.2 为什么 GCC 不崩溃，Keil 崩溃？

| 工具链 | C 库 | `printf` → `fputc` 行为 | 结果 |
|--------|------|-------------------------|------|
| **GCC** (arm-none-eabi) | newlib-nano | `_write()` 是空桩函数，返回 -1 | **静默失败**，无输出但不崩溃 |
| **Keil** (ARMCLANG) | MicroLib (`useUlib=1`) | `fputc()` → semihosting **BKPT 指令** | **HardFault** |

#### Keil MicroLib semihosting 机制

MicroLib 的默认 `fputc` 实现使用 ARM semihosting 协议，通过 `BKPT` 指令与调试器通信。
当没有调试器连接时：

- **Cortex-M3/M4**: `BKPT` 触发 `DebugMonitor` 异常，如果未启用则升级为 HardFault
- **Cortex-M0/M0+** (FM33LG04x): `BKPT` **直接触发 HardFault**（无 DebugMonitor）

从 Keil .map 文件中可以看到 semihosting 链接证据：

```
fputc.o(i.fputc) refers (Special) to iusesemip.o(.text) for __I$use$semihosting$fputc
```

### 2.3 调用链路对比

**修复前（Keil）**：
```
log_i("系统初始化完成")
  → [宏展开] printf("系统初始化完成")
    → fputc() [MicroLib]
      → semihosting BKPT
        → HardFault (无调试器) 💥
```

**修复后（Keil & GCC）**：
```
log_i("系统初始化完成")
  → [宏展开] elog_output(ELOG_LVL_INFO, "main", ..., "系统初始化完成")
    → vsnprintf(log_buf, ...)  // 格式化到缓冲区
    → elog_port_output(log_buf, log_len)
      → Uart0_Tx_Send()       // UART 中断发送 ✅
```

---

## 3. 修复方案

### 修复 1：`Components/FlashDB/fal_cfg.h` — 预定义 `FAL_PRINTF`

在 `fal_def.h` 被包含之前，将 `FAL_PRINTF` 重定向到 `elog_raw_output()`，
避免其默认为 `printf()`：

```c
// fal_cfg.h (在 #include "fal_def.h" 之前被包含)
extern void elog_raw_output(const char *format, ...);
#define FAL_PRINTF(...) elog_raw_output(__VA_ARGS__)
```

**效果**：FAL 库内部的日志也通过 EasyLogger → UART 输出，而非 printf。

### 修复 2：`Components/components.h` — 恢复 EasyLogger 日志宏

在 `fal.h` 包含之后，撤销 `fal_def.h` 对 `log_i` 等宏的覆盖，恢复 EasyLogger 版本：

```c
// components.h — 在 #include "fal.h" 之后
#if defined(COMPONENT_EASYLOGGER) && defined(ELOG_OUTPUT_ENABLE)
  #ifdef log_i
    #undef log_i
  #endif
  #ifdef log_e
    #undef log_e
  #endif
  #ifdef log_d
    #undef log_d
  #endif

  #define log_i(...) elog_i(LOG_TAG, __VA_ARGS__)
  #define log_e(...) elog_e(LOG_TAG, __VA_ARGS__)
  #define log_d(...) elog_d(LOG_TAG, __VA_ARGS__)
#endif
```

**效果**：所有 `#include "components.h"` 之后的文件恢复使用 EasyLogger 通道。

### 修复 3：`Components/EasyLogger/EasyLogger.c` — fputc 兜底（仅 Keil）

此文件仅在 Keil 工程中编译（GCC/CMake 不包含），提供 `fputc` 重写防止残留 `printf` 调用触发 semihosting：

```c
#include <stdio.h>
int fputc(int ch, FILE *f) {
  (void)f;
  return ch;  // 丢弃输出，防止 semihosting HardFault
}
```

**效果**：即使有遗漏的 `printf` 调用，也不会触发 HardFault。

---

## 4. 涉及文件

| 文件 | 操作 | 说明 |
|------|------|------|
| `Components/FlashDB/fal_cfg.h` | 修改 | 添加 `FAL_PRINTF` → `elog_raw_output` 重定向 |
| `Components/components.h` | 修改 | 在 FAL 头文件之后恢复 `log_i`/`log_e`/`log_d` 宏 |
| `Components/EasyLogger/EasyLogger.c` | 修改 | 添加 `fputc` 重写防止 semihosting |

---

## 5. 经验总结

1. **第三方库的宏污染**：FAL/FlashDB 使用 `#undef` + `#define` 强制覆盖同名宏，
   这是一种危险的做法。集成第三方库时必须检查其头文件是否会污染全局宏命名空间。

2. **跨工具链行为差异**：同一段代码在 GCC 和 Keil 下可能表现完全不同。
   `printf` 在 GCC newlib-nano 下是无害的空操作，在 Keil MicroLib 下却是致命的 semihosting 调用。

3. **头文件包含顺序敏感**：`#define` / `#undef` 的效果取决于包含顺序，
   必须确保最终生效的是项目需要的版本。

4. **Keil MicroLib + 无调试器 = semihosting 陷阱**：
   使用 MicroLib 时，如果不重写 `fputc`，任何 `printf` 系列调用都会在无调试器时崩溃。
   建议在所有 Keil 嵌入式项目中始终提供 `fputc` 重写。
