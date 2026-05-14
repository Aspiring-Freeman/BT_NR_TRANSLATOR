# Utility - 通用工具库

## 简介

提供项目通用的工具函数，避免重复实现。

## 模块结构

```
Utility/
├── utility.h           # 统一头文件（只需包含这个）
├── utility_crc.c       # CRC和校验和计算
├── utility_filter.c    # 滤波/去极值算法
├── utility_convert.c   # 数据格式转换
└── README.md           # 本文档
```

## 使用方法

```c
#include "Utility/utility.h"

// 或者如果 Components 已在 include 路径中
#include "utility.h"
```

## 功能列表

### 1. CRC/校验和计算

| 函数 | 说明 |
|------|------|
| `util_crc16_modbus()` | CRC16-Modbus 校验（水表协议） |
| `util_checksum_sum8()` | 8位累加和（PC协议） |
| `util_checksum_sum16()` | 16位累加和 |

### 2. 滤波算法

| 函数 | 说明 | 适用场景 |
|------|------|---------|
| `util_filter_remove_extreme()` | 去极值滤波 | 功耗检测（去掉偶发高值） |
| `util_filter_median()` | 中位数滤波 | 噪声较大的场景 |
| `util_filter_average()` | 简单平均 | 稳定信号 |
| `util_filter_clamp()` | 限幅滤波 | 有明确范围的信号 |

### 3. 数据转换

| 函数 | 说明 |
|------|------|
| `util_read_le_u16()` | 读取小端16位 |
| `util_read_le_u32()` | 读取小端32位 |
| `util_write_le_u16()` | 写入小端16位 |
| `util_write_le_u32()` | 写入小端32位 |
| `util_read_be_u16()` | 读取大端16位 |
| `util_write_be_u16()` | 写入大端16位 |
| `util_bcd_to_dec()` | BCD转十进制 |
| `util_dec_to_bcd()` | 十进制转BCD |
| `util_reverse_bytes()` | 字节数组反转 |
| `util_hex_str_to_bytes()` | 十六进制字符串转字节数组 |

## 示例

### 功耗检测去极值

```c
// 10次采样
uint16_t samples[10] = {24, 21, 54, 37, 29, 25, 22, 45, 41, 32};

// 去掉最高1个和最低1个，用剩余8个计算平均
uint16_t avg = util_filter_remove_extreme(samples, 10, 1, 1);
// 结果: 31 uA (去掉了54和21)
```

### CRC校验

```c
uint8_t data[] = {0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x02};
uint16_t crc = util_crc16_modbus(data, sizeof(data));
```

### 累加和校验

```c
uint8_t frame[] = {0x68, 0xAA, 0x19, 0x00, ...};
uint8_t checksum = util_checksum_sum8(&frame[3], len - 5);  // 从工位到校验和前
```

## 版本历史

### v1.0.0 (2025-12-31)
- 初始版本
- 从各模块抽取通用函数
- 新增去极值滤波算法
