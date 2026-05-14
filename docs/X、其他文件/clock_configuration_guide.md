# 系统时钟配置指南

## 概述

为了解决红外通信稳定性问题，系统提供了灵活的时钟频率配置选项。通过简单的宏定义修改，可以在不同时钟频率之间快速切换。

## 可用时钟频率

| 时钟频率 | 宏定义 | 用途说明 |
|----------|--------|----------|
| 8MHz | `USE_CLOCK_8MHZ` | 红外通信优化模式，可能提高红外通信稳定性 |
| 16MHz | `USE_CLOCK_16MHZ` | 中等性能模式 |
| 24MHz | `USE_CLOCK_24MHZ` | 高性能模式 |
| 32MHz | `USE_CLOCK_32MHZ` | 默认高性能模式 |

## 配置方法

### 文件位置
```
MF-config/Inc/mf_config.h
```

### 配置步骤

1. **打开配置文件**
   ```
   MF-config/Inc/mf_config.h
   ```

2. **找到时钟配置区域**
   ```c
   /* 时钟频率选择 - 请只启用其中一个 */
   // #define USE_CLOCK_8MHZ    // 8MHz - 可能对红外通信更稳定
   // #define USE_CLOCK_16MHZ   // 16MHz
   // #define USE_CLOCK_24MHZ   // 24MHz
   #define USE_CLOCK_32MHZ      // 32MHz - 默认配置
   ```

3. **切换到8MHz（推荐用于红外问题排查）**
   ```c
   /* 时钟频率选择 - 请只启用其中一个 */
   #define USE_CLOCK_8MHZ       // 8MHz - 可能对红外通信更稳定
   // #define USE_CLOCK_16MHZ   // 16MHz
   // #define USE_CLOCK_24MHZ   // 24MHz
   // #define USE_CLOCK_32MHZ   // 32MHz - 默认配置
   ```

4. **重新编译和烧录**
   ```bash
   cmake --build build
   # 然后烧录到设备
   ```

## 自动验证

系统会自动验证配置：
- ✅ 确保只启用一个时钟频率
- ❌ 如果启用多个或零个，编译时会报错

## 调试信息

启动时会显示当前时钟配置：
```
=== 系统时钟配置 ===
软件版本: v1.5.0
系统时钟: 8MHz (红外优化模式)
系统频率: 8000000 Hz
==================
```

## 红外通信优化说明

### 为什么8MHz可能对红外更好？

1. **时序稳定性**：较低的时钟频率可能提供更稳定的时序
2. **电磁干扰**：低频率产生的EMI较小，对红外信号干扰更少
3. **功耗考虑**：8MHz功耗更低，可能减少电源噪声

### 测试建议

如果当前32MHz下红外通信不稳定：

1. **第一步**：切换到8MHz测试
2. **第二步**：如果8MHz有改善，可尝试16MHz
3. **第三步**：找到最佳平衡点（性能vs稳定性）

## 性能影响

| 时钟频率 | 系统性能 | 红外稳定性 | 功耗 | 推荐场景 |
|----------|----------|------------|------|----------|
| 8MHz | 低 | 最高 | 最低 | 红外问题排查 |
| 16MHz | 中等 | 高 | 低 | 平衡模式 |
| 24MHz | 高 | 中等 | 中等 | 高性能需求 |
| 32MHz | 最高 | 中等 | 高 | 默认高性能 |

## 快速切换脚本

为了方便快速切换，可以创建快速配置脚本：

### 切换到8MHz
```bash
# 注释掉32MHz，启用8MHz
sed -i 's/^#define USE_CLOCK_32MHZ/\/\/ #define USE_CLOCK_32MHZ/' MF-config/Inc/mf_config.h
sed -i 's/^\/\/ #define USE_CLOCK_8MHZ/#define USE_CLOCK_8MHZ/' MF-config/Inc/mf_config.h
cmake --build build
```

### 切换回32MHz
```bash
# 注释掉8MHz，启用32MHz
sed -i 's/^#define USE_CLOCK_8MHZ/\/\/ #define USE_CLOCK_8MHZ/' MF-config/Inc/mf_config.h
sed -i 's/^\/\/ #define USE_CLOCK_32MHZ/#define USE_CLOCK_32MHZ/' MF-config/Inc/mf_config.h
cmake --build build
```

## 注意事项

1. **只能选择一个**：同时只能启用一个时钟配置
2. **重新编译**：修改后必须重新编译整个项目
3. **系统性能**：低时钟频率会影响整体系统性能
4. **外设影响**：时钟变化可能影响UART波特率等外设配置

## 故障排除

如果切换时钟后出现问题：

1. **立即回退**：切换回之前的工作配置
2. **检查外设**：确认UART等外设工作正常
3. **逐步测试**：先测试基本功能，再测试红外

这样，您就可以方便地在不同时钟频率之间切换，快速排查红外通信问题！
