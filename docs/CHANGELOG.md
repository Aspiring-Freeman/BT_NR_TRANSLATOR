# Changelog

All notable changes to this project will be documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [v1.1.0] - 2026-05-10

### 重构 · 分层架构全面重写

#### App 层（`App/`）
- 删除旧 `Src/` / `Inc/` 目录及所有旧源文件（anjian、gongnengxiafa、shujudui、lanyaxieyi、BLE_xieyi 等）
- `main.c` 重写为薄编排层：System_Init → Peripheral_Init → while(1) 轮询驱动
- 新增 `app_config.h`：集中管理版本号宏（`APP_VERSION_MAJOR/MINOR/PATCH`）

#### BSP 层（`Bsp/`）
- **UART 全面重写**（NB-IOT 私有缓冲模式）：
  - `Bsp/Peripheral/uart/pc_uart.{h,c}`：PC↔MCU UART0，私有 `s_rx_buf[]`，`PCUart_Rx_Process()` 超时转存，公开 `pc_uart_rx_{data_flag,buffer,count,frame_timeout}`
  - `Bsp/Peripheral/uart/ble_uart.{h,c}`：BLE↔MCU UART4，同等结构，公开 `ble_uart_rx_*`
- `Bsp/GPTIM.c` 精简：仅保留 GPTIM0 1ms 时基 + IRQ Handler，删除所有业务变量
- 删除不再使用的文件：`LPTIM.{c,h}`、`BSTIM.{c,h}`、`ADC_CHK.{c,h}`、`lcd.{c,h}`、`sleep.{c,h}`

#### Protocol 层（`Components/Protocol/`）
- 新增 `protocol_def.h`：帧格式定义（HEAD=0x55 / TAIL=0xAA）、`ProtocolInterface`、校验和宏
- 新增 `protocol_manager.{h,c}`：RegisterPC / SetActivePC / SetPCSendFunc / PC_Parse
- `PC/pc_protocol.h`：PCProtocolCmd 枚举（仅命令定义，业务逻辑外移）
- 新增 `App/BT_Passthrough/bt_passthrough.{h,c}`：
  - `PC_CMD_BT_FORWARD (0x10)` → `BLEUart_Tx_Send()`
  - `PC_CMD_QUERY_VERSION (0xC0)` → 回复 `[MAJOR, MINOR, PATCH]`
  - `PC_CMD_QUERY_STATUS (0x20)` → 回复 `[0x01]`
  - `BT_Passthrough_ForwardBLEToPC()` BLE→PC 透传

#### 构建系统
- `CMakeLists.txt` 改为**显式源文件列表**（不再 glob），避免残留文件污染编译
- 目标名称统一为 `BT_PassThroughFT`
- 新增 `cmake/generate_build_info.cmake`：自动生成 `build/generated/build_info.h`（含版本、时间戳）
- 新增 `.vscode/tasks.json`：Fast Build / Flash / Clean 等任务（Linux & Windows）

#### 工程规范
- 新增 `.gitignore`：排除 `build/`、`build_fast/`、`.clangd`（平台相关）、二进制产物
- 新增 `docs/CHANGELOG.md`（本文件）
- 新增 `VERSION.md`：版本历史说明

### 性能
- Flash 占用：15,964 B / 65,536 B（**24.36%**）
- 编译无 warning，无 error

---

## [v1.0.0] - 2026-05-01

### 初始提交

- 项目骨架：FM33LE015（Cortex-M0，64KB Flash / 16KB RAM）
- 初始文件结构：`Src/`、`Inc/`、`Bsp/`、`Drivers/`、`MF-config/`
- MDK-ARM 工程文件（`MDK-ARM/`）
- FL 驱动库（`Drivers/FM33LE0xx_FL_Driver/`）+ CMSIS
- 基础外设：GPIO、UART0/4、LPTIM、BSTIM、ADC、LCD、Flash
- CMakeLists.txt 初始配置（glob 源文件）
