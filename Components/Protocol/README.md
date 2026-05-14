# Protocol 协议框架组件

## 简介

本组件提供模块化的协议管理框架，支持上位机协议(PC Protocol)和下位机/设备协议(Device Protocol)的灵活配置组合。

## 目录结构

```
Protocol/
├── README.md                    # 本文档
├── protocol.h                   # 统一对外接口（只需包含这个）
├── protocol_def.h               # 协议公共定义
├── protocol_manager.h           # 协议管理器接口
├── protocol_manager.c           # 协议管理器实现
├── PC/                          # 上位机协议 (MES系统)
│   ├── pc_protocol.h            # PC协议接口定义
│   ├── pc_protocol_mes.c        # MES系统协议实现
│   └── pc_protocol_template.c.example  # 协议模板
└── Device/                      # 下位机协议 (表类设备)
    ├── device_protocol.h        # 设备协议接口定义
    ├── device_protocol_water_meter.c  # 水表协议实现
    └── device_protocol_template.c.example  # 协议模板
```

## 快速开始

### 1. 包含头文件

```c
#include "Protocol/protocol.h"
```

### 2. 初始化协议系统

```c
// 方式1: 完整初始化（推荐）
Protocol_InitWithDefaults();

// 方式2: 分步初始化
Protocol_Init();
Protocol_RegisterPC(&mes_pc_protocol);
Protocol_RegisterDevice(&water_meter_protocol);
```

### 3. 设置硬件发送函数

```c
// 设置PC串口发送函数
Protocol_SetPCSendFunc(uart0_send);

// 设置设备串口发送函数
Protocol_SetDeviceSendFunc(uart1_send);
```

### 4. 处理接收数据

```c
// 在PC串口接收中断/回调中
void PC_UART_RX_Callback(uint8_t *data, uint16_t len) {
    Protocol_PC_Parse(data, len);
}

// 在设备串口接收中断/回调中
void Device_UART_RX_Callback(uint8_t *data, uint16_t len) {
    Protocol_Device_Parse(data, len);
}
```

## 核心设计

### 协议抽象层

每种协议实现一个标准接口：

```c
typedef struct {
    const char *name;                           // 协议名称
    bool (*init)(void);                         // 初始化
    ProtocolResult (*parse)(uint8_t *data, uint16_t len); // 解析数据
    bool (*send_cmd)(uint16_t cmd, void *param);     // 发送命令
    void (*on_response)(uint16_t code, const uint8_t *data, uint16_t len);
    void (*set_send_func)(ProtocolSendFunc send_func);
    void (*set_event_callback)(ProtocolEventCallback callback);
} ProtocolInterface;
```

### 运行时切换协议

```c
// 切换到指定协议
Protocol_SetActivePC("mes");
Protocol_SetActiveDevice("water_meter");

// 查看当前协议
ProtocolManager_PrintInfo();
```

## 添加新协议

### 步骤1: 复制模板

```bash
# 添加新的上位机协议
cp PC/pc_protocol_template.c.example PC/pc_protocol_xxx.c

# 添加新的设备协议
cp Device/device_protocol_template.c.example Device/device_protocol_xxx.c
```

### 步骤2: 实现协议接口

修改复制的文件，实现以下核心函数：
- `init()` - 初始化协议
- `parse()` - 解析接收数据
- `send_cmd()` - 发送命令

### 步骤3: 声明协议实例

在 `pc_protocol.h` 或 `device_protocol.h` 中添加：

```c
extern const ProtocolInterface xxx_protocol;
```

### 步骤4: 注册协议

在 `protocol.h` 的 `Protocol_RegisterDefaults()` 中添加：

```c
Protocol_RegisterPC(&xxx_protocol);
// 或
Protocol_RegisterDevice(&xxx_protocol);
```

## 已实现协议

### 上位机协议 (MES系统)

| 名称 | 文件 | 描述 |
|------|------|------|
| legacy | pc_protocol_legacy.c | Legacy适配层，对接现有PC_xieyi_Ctrl.c |
| upgrade | pc_protocol_upgrade.c | APP固件升级协议 (0xBA/0xBB) |
| mes | pc_protocol_mes.c | MES系统通信协议 (重新实现版本) |

### 下位机协议 (表类设备)

| 名称 | 文件 | 描述 |
|------|------|------|
| water_meter | device_protocol_water_meter.c | 水表通信协议 |

## 推荐使用方式

### 方式1: 使用Legacy适配层 (推荐，兼容现有代码)

```c
#include "Protocol/protocol.h"

// 初始化，使用现有的 PC_xieyi_Ctrl.c 代码
Protocol_InitWithLegacy();
```

### 方式2: 完全使用新框架

```c
#include "Protocol/protocol.h"

Protocol_InitWithDefaults();
Protocol_SetActivePC("mes");  // 使用新实现的MES协议
```

## API 参考

### 初始化

| 函数 | 描述 |
|------|------|
| `Protocol_Init()` | 初始化协议管理器 |
| `Protocol_InitWithLegacy()` | 初始化并使用Legacy适配层 (推荐) |
| `Protocol_InitWithDefaults()` | 初始化并注册默认协议 |

### 协议注册

| 函数 | 描述 |
|------|------|
| `Protocol_RegisterPC(p)` | 注册上位机协议 |
| `Protocol_RegisterDevice(p)` | 注册设备协议 |

### 数据处理

| 函数 | 描述 |
|------|------|
| `Protocol_PC_Parse(d, l)` | 解析上位机数据 |
| `Protocol_Device_Parse(d, l)` | 解析设备数据 |
| `Protocol_PC_SendCmd(c, p)` | 发送上位机命令 |
| `Protocol_Device_SendCmd(c, p)` | 发送设备命令 |

### 配置

| 函数 | 描述 |
|------|------|
| `Protocol_SetPCSendFunc(f)` | 设置PC发送函数 |
| `Protocol_SetDeviceSendFunc(f)` | 设置设备发送函数 |
| `Protocol_SetActivePC(name)` | 切换上位机协议 |
| `Protocol_SetActiveDevice(name)` | 切换设备协议 |

## 版本历史

- v1.0.0 (2024-12) - 初始版本
  - 协议管理器框架
  - MES系统上位机协议
  - 水表下位机协议
