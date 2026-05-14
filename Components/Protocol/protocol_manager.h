/**
 * @file protocol_manager.h
 * @brief 协议管理器 - 统一管理上位机和设备协议
 * @version 1.0.0
 * @date 2024-12
 *
 * @section intro 简介
 * 协议管理器负责注册、管理和调度各种协议实现。
 * 支持运行时切换协议，实现协议的灵活组合。
 *
 * @section usage 使用方法
 *
 * 1. 初始化:
 * @code
 * ProtocolManager_Init();
 * @endcode
 *
 * 2. 注册协议:
 * @code
 * ProtocolManager_RegisterPC(&mes_pc_protocol);
 * ProtocolManager_RegisterDevice(&water_meter_protocol);
 * @endcode
 *
 * 3. 设置活跃协议:
 * @code
 * ProtocolManager_SetActivePC("mes");
 * ProtocolManager_SetActiveDevice("water_meter");
 * @endcode
 *
 * 4. 处理接收数据:
 * @code
 * ProtocolManager_PC_Parse(rx_buf, rx_len);
 * ProtocolManager_Device_Parse(rx_buf, rx_len);
 * @endcode
 */

#ifndef __PROTOCOL_MANAGER_H__
#define __PROTOCOL_MANAGER_H__

#include "protocol_def.h"

/*============ 管理器初始化 ============*/

/**
 * @brief 初始化协议管理器
 *
 * 必须在使用其他功能前调用
 */
void ProtocolManager_Init(void);

/*============ 协议注册接口 ============*/

/**
 * @brief 注册上位机协议
 * @param protocol 协议接口指针
 * @return 成功返回true
 */
bool ProtocolManager_RegisterPC(const ProtocolInterface *protocol);

/**
 * @brief 注册设备协议
 * @param protocol 协议接口指针
 * @return 成功返回true
 */
bool ProtocolManager_RegisterDevice(const ProtocolInterface *protocol);

/*============ 协议切换接口 ============*/

/**
 * @brief 设置当前活跃的上位机协议
 * @param name 协议名称
 * @return 成功返回true
 */
bool ProtocolManager_SetActivePC(const char *name);

/**
 * @brief 设置当前活跃的设备协议
 * @param name 协议名称
 * @return 成功返回true
 */
bool ProtocolManager_SetActiveDevice(const char *name);

/**
 * @brief 获取当前活跃的上位机协议名称
 * @return 协议名称，未设置返回NULL
 */
const char *ProtocolManager_GetActivePCName(void);

/**
 * @brief 获取当前活跃的设备协议名称
 * @return 协议名称，未设置返回NULL
 */
const char *ProtocolManager_GetActiveDeviceName(void);

/*============ 协议查询接口 ============*/

/**
 * @brief 获取已注册的上位机协议数量
 * @return 协议数量
 */
uint8_t ProtocolManager_GetPCCount(void);

/**
 * @brief 获取已注册的设备协议数量
 * @return 协议数量
 */
uint8_t ProtocolManager_GetDeviceCount(void);

/**
 * @brief 根据索引获取上位机协议名称
 * @param index 索引
 * @return 协议名称，无效索引返回NULL
 */
const char *ProtocolManager_GetPCNameByIndex(uint8_t index);

/**
 * @brief 根据索引获取设备协议名称
 * @param index 索引
 * @return 协议名称，无效索引返回NULL
 */
const char *ProtocolManager_GetDeviceNameByIndex(uint8_t index);

/*============ 数据处理接口 ============*/

/**
 * @brief 解析上位机数据
 *
 * 使用当前活跃的上位机协议解析数据
 *
 * @param data 接收缓冲区
 * @param len 数据长度
 * @return 解析结果
 */
ProtocolResult ProtocolManager_PC_Parse(uint8_t *data, uint16_t len);

/**
 * @brief 解析设备数据
 *
 * 使用当前活跃的设备协议解析数据
 *
 * @param data 接收缓冲区
 * @param len 数据长度
 * @return 解析结果
 */
ProtocolResult ProtocolManager_Device_Parse(uint8_t *data, uint16_t len);

/*============ 命令发送接口 ============*/

/**
 * @brief 通过上位机协议发送命令
 * @param cmd 命令码
 * @param param 参数(可选)
 * @return 成功返回true
 */
bool ProtocolManager_PC_SendCmd(uint16_t cmd, void *param);

/**
 * @brief 通过设备协议发送命令
 * @param cmd 命令码
 * @param param 参数(可选)
 * @return 成功返回true
 */
bool ProtocolManager_Device_SendCmd(uint16_t cmd, void *param);

/*============ 响应处理接口 ============*/

/**
 * @brief 通知上位机协议收到响应
 * @param code 响应码
 * @param data 响应数据
 * @param len 数据长度
 */
void ProtocolManager_PC_OnResponse(uint16_t code, const uint8_t *data,
                                   uint16_t len);

/**
 * @brief 通知设备协议收到响应
 * @param code 响应码
 * @param data 响应数据
 * @param len 数据长度
 */
void ProtocolManager_Device_OnResponse(uint16_t code, const uint8_t *data,
                                       uint16_t len);

/*============ 硬件绑定接口 ============*/

/**
 * @brief 设置上位机协议的发送函数
 * @param send_func 发送函数指针
 */
void ProtocolManager_SetPCSendFunc(ProtocolSendFunc send_func);

/**
 * @brief 设置设备协议的发送函数
 * @param send_func 发送函数指针
 */
void ProtocolManager_SetDeviceSendFunc(ProtocolSendFunc send_func);

/*============ 调试接口 ============*/

/**
 * @brief 打印已注册的所有协议信息
 */
void ProtocolManager_PrintInfo(void);

#endif /* __PROTOCOL_MANAGER_H__ */
