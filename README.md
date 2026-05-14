# BT PassThrough 功能测试系统

基于 FM33LE015 微控制器的蓝牙透传功能测试平台。

## 系统架构

```
┌──────────────────────────────────────┐
│              PC 上位机                │
└──────────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────┐
│           FM33LE015 MCU              │
│  ┌──────────────┐  ┌──────────────┐  │
│  │  功能下发控制 │  │   BLE 协议   │  │
│  │  gongnengxiafa  │  lanyaxieyi  │  │
│  └──────┬───────┘  └──────┬───────┘  │
│         │                 │          │
│  ┌──────┴─────────────────┴───────┐  │
│  │         Bsp 板级驱动层          │  │
│  │  GPIO / UART / LCD / ADC / 按键 │  │
│  └────────────────────────────────┘  │
│  ┌────────────────────────────────┐  │
│  │    FM33LE0xx FL Driver / HAL   │  │
│  └────────────────────────────────┘  │
└──────────────────────────────────────┘
        │                │
        ▼                ▼
   ┌────────┐       ┌─────────┐
   │ UART0  │       │  UART1  │
   │ (PC)   │       │ (BLE模组)│
   └────────┘       └─────────┘
```

## 目录结构

```
BT_PassThroughFT/
├── App/                        # 应用层
│   ├── main.c / main.h         # 主入口
│   ├── gongnengxiafa.*         # 功能下发控制
│   ├── lanyaxieyi.*            # 蓝牙协议
│   ├── BLE_xieyi.*             # BLE 通信
│   ├── anjian.*                # 按键处理
│   └── shujudui.*              # 数据队列
├── Bsp/                        # 板级支持层
│   ├── GPIO.*                  # GPIO 驱动
│   ├── uart0.* / uart1.*       # UART 驱动
│   ├── ADC_CHK.*               # ADC 检测
│   ├── lcd.*                   # LCD 显示
│   ├── flash.*                 # Flash 读写
│   ├── sleep.*                 # 低功耗睡眠
│   ├── BSTIM.* / GPTIM.*       # 定时器
│   ├── LPTIM.* / EXTI.*        # 低功耗定时器 / 外部中断
│   └── anjian.*                # 按键 BSP
├── Drivers/                    # FM33LE0xx FL 驱动库 + CMSIS
│   ├── CMSIS/Device/           # CMSIS 设备头文件
│   └── FM33LE0xx_FL_Driver/    # 复旦微 FL 外设驱动
├── MF-config/                  # 外设初始化配置（MF工具生成）
├── VscodeGcc/                  # SVD 文件 + DFP Pack
├── cmake/                      # CMake 辅助脚本
├── CMakeLists.txt              # CMake 主构建文件
├── VERSION.md                  # 版本历史
└── README.md                   # 本文件
```

## 开发环境

| 工具 | 版本要求 | 用途 |
|------|---------|------|
| ARM GCC | 10.0+ | 交叉编译工具链 |
| CMake | 3.16+ | 构建系统 |
| PyOCD | 0.34+ | 调试与烧录 |

## 构建

```bash
# 配置（首次）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 编译
cmake --build build --parallel
```

## 烧录

```bash
pyocd flash build/BT_PassThroughFT.elf \
    --target fm33le01x \
    --frequency 1000000 \
    --pack VscodeGcc/FM33LE0XX_DFP.1.0.2.pack
```

## 许可证

内部项目，版权归前锋电子所有。
