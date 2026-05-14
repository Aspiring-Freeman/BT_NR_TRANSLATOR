# 版本历史

## v1.1.0 (2026-05-10) - 分层架构重构

### 项目信息
- 芯片：FM33LE015（Cortex-M0，64KB Flash，16KB RAM）
- 项目：BT PassThrough 功能测试系统

### 架构
- `App/`：薄编排层（main / app_config）
- `Bsp/`：纯硬件驱动（GPIO / UART / GPTIM，NB-IOT 私有缓冲模式）
- `Components/Protocol/`：协议管理 + BT 透传实现
- `Drivers/`：FM33LE0xx FL 驱动库 + CMSIS
- `MF-config/`：外设初始化配置

详细变更记录见 `docs/CHANGELOG.md`。

---

## v1.0.0 (2026-05-01) - 初始版本

### 初始结构
- `App/`：应用层（main / 功能下发 / BLE协议 / 按键 / 数据队列）
- `Bsp/`：板级驱动（GPIO / UART / LCD / ADC / 定时器 / Flash / 睡眠）
- `Drivers/`：FM33LE0xx FL 驱动库 + CMSIS
- `MF-config/`：外设初始化配置
- CMakeLists.txt、.vscode 配置均适配 FM33LE015
