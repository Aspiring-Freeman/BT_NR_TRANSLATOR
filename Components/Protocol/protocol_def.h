/**
 * @file protocol_def.h
 * @brief 协议框架公共定义
 * @version 1.0.0
 * @date 2024-12
 */

#ifndef __PROTOCOL_DEF_H__
#define __PROTOCOL_DEF_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// 引入 utility 的 inline 函数
#include "utility_inline.h"

/*============ 协议类型定义 ============*/

/**
 * @brief 协议类型枚举
 */
typedef enum {
  PROTOCOL_TYPE_PC = 0, // 上位机协议
  PROTOCOL_TYPE_DEVICE, // 设备/下位机协议
  PROTOCOL_TYPE_MAX
} ProtocolType;

/**
 * @brief 协议解析结果
 */
typedef enum {
  PROTOCOL_RESULT_OK = 0,         // 解析成功
  PROTOCOL_RESULT_INCOMPLETE,     // 数据不完整，等待更多数据
  PROTOCOL_RESULT_INVALID_HEAD,   // 无效帧头
  PROTOCOL_RESULT_INVALID_TAIL,   // 无效帧尾
  PROTOCOL_RESULT_CHECKSUM_ERROR, // 校验和错误
  PROTOCOL_RESULT_LENGTH_ERROR,   // 长度错误
  PROTOCOL_RESULT_UNKNOWN_CMD,    // 未知命令
  PROTOCOL_RESULT_ERROR           // 其他错误
} ProtocolResult;

/**
 * @brief 协议回调事件类型
 */
typedef enum {
  PROTOCOL_EVENT_RECEIVED,       // 收到有效帧
  PROTOCOL_EVENT_SENT,           // 发送完成
  PROTOCOL_EVENT_ERROR,          // 错误
  PROTOCOL_EVENT_TIMEOUT,        // 超时
  PROTOCOL_EVENT_UPGRADE_REQUEST // 升级请求
} ProtocolEvent;

/*============ 协议接口定义 ============*/

/**
 * @brief 协议发送函数类型
 * @param data 要发送的数据
 * @param len 数据长度
 *
 * 注意: 使用 void 返回类型以兼容现有的 Uart0_Tx_Send 等函数
 */
typedef void (*ProtocolSendFunc)(uint8_t *data, uint16_t len);

/**
 * @brief 协议事件回调函数类型
 * @param event 事件类型
 * @param cmd 命令码
 * @param data 相关数据
 * @param len 数据长度
 */
typedef void (*ProtocolEventCallback)(ProtocolEvent event, uint16_t cmd,
                                      const uint8_t *data, uint16_t len);

/**
 * @brief 前导码配置结构体
 *
 * 用于配置协议发送前的前导码（如水表的唤醒前导）
 */
typedef struct {
  bool enabled;             ///< 是否启用前导
  const uint8_t *data;      ///< 前导数据
  uint16_t length;          ///< 前导数据长度
  uint8_t repeat_count;     ///< 重复发送次数
  uint8_t delay_ms;         ///< 每次发送后延时(ms)
  const uint8_t *sync_data; ///< 同步/校准前导数据(可选)
  uint16_t sync_length;     ///< 同步前导长度
} ProtocolPreambleConfig;

/**
 * @brief 协议接口结构体
 *
 * 每种协议实现必须提供这些接口
 */
typedef struct {
  const char *name; // 协议名称标识符

  /**
   * @brief 初始化协议
   * @return 成功返回true
   */
  bool (*init)(void);

  /**
   * @brief 解析接收到的数据
   * @param data 接收缓冲区
   * @param len 数据长度
   * @return 解析结果
   */
  ProtocolResult (*parse)(uint8_t *data, uint16_t len);

  /**
   * @brief 发送命令
   * @param cmd 命令码
   * @param param 命令参数(可选)
   * @return 成功返回true
   */
  bool (*send_cmd)(uint16_t cmd, void *param);

  /**
   * @brief 处理响应
   * @param code 响应码
   * @param data 响应数据
   * @param len 数据长度
   */
  void (*on_response)(uint16_t code, const uint8_t *data, uint16_t len);

  /**
   * @brief 设置发送函数
   * @param send_func 硬件发送函数
   */
  void (*set_send_func)(ProtocolSendFunc send_func);

  /**
   * @brief 设置事件回调
   * @param callback 事件回调函数
   */
  void (*set_event_callback)(ProtocolEventCallback callback);

  /**
   * @brief 前导码配置 (可选)
   * @note 匇向静态前导配置，NULL表示不需要前导
   */
  const ProtocolPreambleConfig *preamble;

} ProtocolInterface;

/*============ 协议帧公共定义 ============*/

// 常用帧头帧尾,工装协议使用自定义帧头帧尾避免冲突
#define FRAME_HEAD_BLE 0xFC
#define FRAME_TAIL_BLE 0xFB
// 工装特殊帧头，自定义协议，避免和其他协议冲突
#define FT_FRAME_HEAD 0x55
#define FT_FRAME_TAIL 0xAA

// 最大协议名称长度
#define PROTOCOL_NAME_MAX_LEN 32

// 最大注册协议数量
#define MAX_REGISTERED_PROTOCOLS 8

/*============ 工具宏定义 ============*/

// 小端/大端读写 - 使用 utility_inline.h 中的 inline 函数
// 保持宏名兼容，方便现有代码使用
#define READ_LE_U16(buf) util_read_le_u16(buf)
#define WRITE_LE_U16(buf, val) util_write_le_u16(buf, val)
#define READ_BE_U16(buf) util_read_be_u16(buf)
#define WRITE_BE_U16(buf, val) util_write_be_u16(buf, val)

// 32位版本
#define READ_LE_U32(buf) util_read_le_u32(buf)
#define WRITE_LE_U32(buf, val) util_write_le_u32(buf, val)
#define READ_BE_U32(buf) util_read_be_u32(buf)
#define WRITE_BE_U32(buf, val) util_write_be_u32(buf, val)

#endif /* __PROTOCOL_DEF_H__ */
