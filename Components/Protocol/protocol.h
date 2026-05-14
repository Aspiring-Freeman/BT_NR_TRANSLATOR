/**
 * @file protocol.h
 * @brief 协议组件统一对外接口
 * @version 1.0.0
 * @date 2024-12
 *
 * @section intro 简介
 * 只需包含此头文件即可使用协议框架的所有功能。
 *
 * @section usage 使用示例
 *
 * @code
 * #include "Protocol/protocol.h"
 *
 * // 初始化
 * Protocol_Init();
 *
 * // 设置硬件发送函数
 * Protocol_SetPCSendFunc(uart0_send);
 * Protocol_SetDeviceSendFunc(uart1_send);
 *
 * // 处理接收数据
 * Protocol_PC_Parse(pc_rx_buf, pc_rx_len);
 * Protocol_Device_Parse(device_rx_buf, device_rx_len);
 * @endcode
 */

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "PC/pc_protocol.h"
#include "Protocol/App/BT_Passthrough/bt_app_proto.h"
#include "Protocol/App/BT_Passthrough/bt_ds809.h"
#include "protocol_def.h"
#include "protocol_manager.h"

/*============ 简化接口宏 ============*/

// 初始化协议系统
#define Protocol_Init() ProtocolManager_Init()

// 注册协议
#define Protocol_RegisterPC(p) ProtocolManager_RegisterPC(p)
#define Protocol_RegisterDevice(p) ProtocolManager_RegisterDevice(p)

// 设置活跃协议
#define Protocol_SetActivePC(name) ProtocolManager_SetActivePC(name)
#define Protocol_SetActiveDevice(name) ProtocolManager_SetActiveDevice(name)

// 数据解析
#define Protocol_PC_Parse(d, l) ProtocolManager_PC_Parse(d, l)
#define Protocol_Device_Parse(d, l) ProtocolManager_Device_Parse(d, l)

// 发送命令
#define Protocol_PC_SendCmd(c, p) ProtocolManager_PC_SendCmd(c, p)
#define Protocol_Device_SendCmd(c, p) ProtocolManager_Device_SendCmd(c, p)

// 设置发送函数
#define Protocol_SetPCSendFunc(f) ProtocolManager_SetPCSendFunc(f)
#define Protocol_SetDeviceSendFunc(f) ProtocolManager_SetDeviceSendFunc(f)

// 调试信息
#define Protocol_PrintInfo() ProtocolManager_PrintInfo()

/*============ 默认协议注册 ============*/

/**
 * @brief 注册所有默认协议
 *
 * 调用此函数注册框架内置的所有协议实现
 */
static inline void Protocol_RegisterDefaults(void) {
  // 注册Legacy适配层协议 (对接现有 PC_xieyi_Ctrl.c)
  Protocol_RegisterPC(&legacy_pc_protocol);

  // 注册APP升级协议
  Protocol_RegisterPC(&upgrade_pc_protocol);

  // 注册水表PC协议
  // Protocol_RegisterPC(&water_meter_pc_protocol);

  // 注册膜式燃气表PC协议，暂时先用这个，后续需要注册另外协议，当前先测试，后续迁移
  Protocol_RegisterPC(&diaphragm_gas_meter_pc_protocol);

  // 注册水表设备协议
  // Protocol_RegisterDevice(&water_meter_protocol);

  // 注册膜式燃气表设备协议
  // Protocol_RegisterDevice(&diaphragm_gas_meter_protocol);

  // 注册蓝牙应用层协议 (BB01/BB02/AAFF)，需要时由 ble.c 主动注册并切换为活跃
  // Protocol_RegisterDevice(&bt_app_protocol);

  // 注册蓝牙通讯协议（本工程已统一使用 config_pc_protocol，
  // 由 main.c 显式注册，这里不再重复）
}

/**
 * @brief 使用Legacy协议初始化 (推荐，兼容现有代码)
 *
 * 初始化协议管理器，使用现有的 PC_xieyi_Ctrl.c 代码
 */
static inline void Protocol_InitWithLegacy(void) {
  Protocol_Init();
  Protocol_RegisterDefaults();
  // 设置legacy为活跃协议
  Protocol_SetActivePC("legacy");
}

/**
 * @brief 完整初始化协议系统
 *
 * 初始化管理器并注册所有默认协议
 */
static inline void Protocol_InitWithDefaults(void) {
  Protocol_Init();
  Protocol_RegisterDefaults();
}

#endif /* __PROTOCOL_H__ */
