/**
 * @file    bt_passthrough.h
 * @brief   BT 透传协议接口
 *
 * 实现了从 PC 下发数据透传给 BLE 模组，以及将 BLE 应答回传给 PC 的逻辑。
 * 后续若蓝牙改为连接手机 App，只需扩展此模块而无需改动 PC 协议层。
 */
#ifndef __BT_DS809_H__
#define __BT_DS809_H__

#include <stdint.h>

#ifndef __DEVICE_PROTOCOL_DIAPHRAGM_GAS_METER_EVENTS_H__
#define __DEVICE_PROTOCOL_DIAPHRAGM_GAS_METER_EVENTS_H__

#include <stdbool.h>
#include <stdint.h>

/*============================================================================
 *                          事件类型定义
 *===========================================================================*/

// 蓝牙协议事件类型，主要看控制码
typedef enum {
  BLE_EVENT_TYPE_NONE_EVENT = 0, // 无事件

  // 例外的响应事件，蓝牙模组发起的请求协议的响应，和当前任何协议都不兼容，单独处理，这是老的协议，实际是写协议
  BLE_EVENT_SET_BROADCASET_SELF_DEFINE_DATA_EVENT, // 设置广播自定义数据,0xB008

  // 模组上电期间主动发起的请求事件
  BLE_EVENT_CONFIGURE_UUID_EVENT,      // 配置UUID,0xB00D
  BLE_EVENT_CONFIGURE_BLUETOOTH_EVENT, // 配置工作参数，0xB007

  // 读取,mcu发送读取参数协议后，模组响应的事件
  BLE_EVENT_GET_CURRENT_IF_CONNECTED_EVENT, // 获取当前是否连接,0xB00E
  BLE_EVENT_GET_BLE_MAC_ADDRESS_EVENT,      // 获取蓝牙MAC地址,0xC001
  BLE_EVENT_DYAMIC_UPDATE_SCAN_RESPONSE_DATA_EVENT, // 动态更新扫描响应数据,0xB015
  BLE_EVENT_GET_FIRMWARE_VERSION_EVENT,             // 获取固件版本,0xB011

  // 设置,mcu发送设置参数协议后，模组响应的事件
  BLE_EVENT_SET_BROADCAST_PARAMS_EVENT,         // 设置广播参数,0xB012
  BLE_EVENT_SET_EMMIT_POWER_EVENT,              // 设置发射功率,0xB013
  BLE_EVENT_DYAMIC_UPDATE_BROADCAST_DATA_EVENT, // 动态更新广播数据,0xB014
  BLE_EVENT_SET_BLUETOOTH_SYSTEM_STATUS_EVENT, // 设置蓝牙系统状态,0xB000

  /*错误事件*/
  BLE_EVENT_PARSE_ERROR_EVENT,    // 解析错误
  BLE_EVENT_CHECKSUM_ERROR_EVENT, // 校验和错误
  BLE_EVENT_TIMEOUT_EVENT,        // 超时

  BLE_EVENT_MAX, // 枚举边界，不作事件使用
} BLE_EventType;

/*============================================================================
 *                          事件数据结构定义
 *===========================================================================*/
// 模组主动请求事件
// 都一样，数据域是空的，不需要解析，所以不定义数据结构了，直接在事件数据里放一个空的占位符就行了
// 不用配置事件

// 设置事件的响应
/**
 * @brief 设置系统状态的响应 (0xB000 响应, 1字节
  设置系统复位命令被模组接收后，系统执行软
 件复位，并重新发起请求命令 （0xB007/0xB00D）；
 唤醒深度睡眠的系统，只需要发送一帧串口数据即可。
 )
 *@param bluetooth_system_status 蓝牙系统状态 (1是设置成功, 0是设置失败)
 */
typedef struct {
  uint8_t bluetooth_system_status;
} BleSetSystemStatusResponse;

/**
 * @brief  设置广播数据中用户自定义的数据块（旧版本接口）的响应,4字节
 * @param custom_data_status 用户自定义数据块状态 (1是设置成功, 0是设置失败)
 *
 */
typedef struct {
  uint32_t custom_data_status;
} BleSetCustomDataResponse;

/**
 * @brief 设置广播参数的响应，0xB012,3字节
 * @param broadcasting_mode 广播模式 (0x00=停止广播; 0x01=缺省慢速广播,2秒
 * 0x02=高速广播,若设置不带【参数 2】，则缺省以 45ms
 * 毫秒间隔持续广播；
 * @param broadcasting_time_interval 广播时间间隔
 * (单位ms,2字节，可设置取值 范围 20~5000ms。 )·
 * 【举例】：  设定广播参数为缺省快速广播（45ms）： FC
 * AA FC 13 03 12 B0 02 7C FB 设定广播参数为缺省慢速广播（2000ms）： FC AA FC 13
 * 03 12 B0 01 7B FB 模块响应： FC AA FC 93 03 12 B0 00 FA FB
 */
typedef struct {
  uint8_t broadcasting_mode; // 广播模式
  // (单位0.625ms,比如0x20表示20*0.625=12.5ms)
  uint8_t broadcasting_time_interval[2]; // 广播时间间隔
} BleSetBroadcastParamsResponse;

/**
 * @brief 设置发射功率响应,1字节，0xB013 ，DB809/BB810/BB832 最大功率值为+4dBm
 *
 */
typedef struct {
  uint8_t emit_power; // 发射功率 (0x00=最小功率, 0x01=中等功率, 0x02=最大功率)
} BleSetEmitPowerResponse;

/**
 * @brief 动态更新/查询广播数据x的响应,0xB014, 3~31字节,响应1字节
 * @param update_status 更新状态 (0是设置成功, 1是设置失败)
 *
 */
typedef struct {
  uint8_t update_status; // 更新状态
} BleDynamicUpdateBroadcastDataResponse;

// 获取参数事件的响应
/**
 * @brief 获取当前的蓝牙固件的版本号(0xB001,3字节数据)
 * @detail 数据域内容:0x01 0x12 0x01 表示 V1.18.1，内部第 1 次编译发行版本
 *
 */
typedef struct {
  uint8_t major; // 主版本号高位
  uint8_t minor; // 主版本号低位
  uint8_t patch; // 编译发布次数
} BleGetFirmwareVersionResponse;

/**
 * @brief 获取当前是否连接的响应 (0xB00E 响应, 1字节)
 * @detail 数据域内容: 0x00=未连接, 0x01=已连接
 *
 */
typedef struct {
  uint8_t connected;
} BleGetConnectionStatusResponse;

/**
 * @brief  动态更新/查询扫描响应数据 0xB015
 * @param update_status 更新状态 (0是设置成功, 1是设置失败)
 *
 */
typedef struct {
  uint8_t update_status;
} BleDynamicUpdateScanResponseDataResponse;

/**
 * @brief 获取蓝牙MAC地址的响应 (0xC001 响应, 6字节)
 * @detail 数据域内容: 6字节MAC地址, 大端模式
 *
 */
typedef struct {
  uint8_t mac_address[6]; // MAC地址 (6字节) ,大端模式
} BleGetBleMacAddressResponse;

//----------------------------------------------------------------------------------------------
// 配置系统响应(0xB007 响应,蓝牙模组向主控 MCU 请 求 MAC
// 地址、PSW,此处数据域是空，因为这个是蓝牙模组向主控 MCU 请求数据的协议，主控
// MCU
// 收到这个协议后需要回复蓝牙模组当前的MAC地址、配对密码和广播设备名称等参数，所以这个协议是蓝牙模组发起的请求协议，主控
// MCU 收到这个协议后需要回复数据，所以这个协议的响应数据域是空的 )
// 配对密码、Name 广播设备名称等参数) 配置UUID的响应 (0xB00D)
// 响应,略过,蓝牙请求的数据域是空 )

/**
 * @brief 事件数据联合体，蓝牙返回的数据数据域
 */
typedef union {
  // 配置工作参数响应，数据域空，不需要解析
  // 配置UUID响应，数据域空，不需要解析
  BleSetSystemStatusResponse set_system_status_response; // 设置系统状态的响应
  BleSetCustomDataResponse set_custom_data_response; // 设置广播自定义数据的响应
  BleGetConnectionStatusResponse
      get_connection_status_response; // 获取连接状态的响应
  BleGetFirmwareVersionResponse
      get_firmware_version_response; // 获取固件版本的响应
  BleSetBroadcastParamsResponse
      set_broadcast_params_response; // 设置广播参数的响应
  BleSetEmitPowerResponse set_emit_power_response; // 设置发射功率的响应
  BleDynamicUpdateBroadcastDataResponse
      dynamic_update_broadcast_data_response; // 动态更新查询广播数据的响应
  BleDynamicUpdateScanResponseDataResponse
      dynamic_update_scan_response_data_response; // 动态更新查询扫描响应数据的响应
  BleGetBleMacAddressResponse
      get_ble_mac_address_response; // 获取蓝牙MAC地址的响应
  uint8_t raw_data
      [50]; // 最大空间，确保足够存放任何事件的原始数据，便于调试日志输出，当前没有事件会超过50字节
} BleEventData_t;

/**
 * @brief 协议事件结构体
 */
typedef struct {
  BLE_EventType type;  // 事件类型
  uint16_t data_mark;  // 原始数据标识 (用于调试)
  BleEventData_t data; // 事件数据
} BleProtocolEvent;

// 这是mcu对蓝牙的组的响应，不是蓝牙模组对mcu的响应，和以上所有的事件需要区分，这个不是事件
//  请求事件响应
/**
 * @brief 配置工作参数的响应(B007),蓝牙模组向主控 MCU 请 求 MAC 地址、PSW 配对
 * 密码、Name 广播设备名称等参数
 *@note
 *这个是唯二条蓝牙主动发起的协议，发给mcu，mcu收到这个协议后需要回复蓝牙模组当前的MAC地址、配对密码和广播设备名称等参数
 *
 */
typedef struct {
  uint8_t mac_address[6]; // MAC地址 (6字节) ,大端模式
  uint8_t
      pairing_password[4]; // 配对密码 ，4字节无符号整数，小端模式，比如40 E2
                           // 01 00
                           // ，实际密码是123456,如果不设置密码，使用4字节0xFF
  char device_name[8]; // 广播设备名称 (8字节字符串, 不包含终止符)
  uint8_t broadcast_status; // 广播状态 (0x00=关闭广播, 0x01=开启广播)
} BleQueryConfigureBluetoothResponse;

/**
 * @brief
 * 配置蓝牙uuid的响应，模组上电请求，mcu回复这个协议，数据域最好配置下，否则一直在发送请求，需要等它超时
 *
 */
#pragma pack(push, 1) // 要限制下，否则里面字节有问题
typedef struct {
  uint8_t uuid_attribute;          //
  uint8_t service_uuid[2];         // 服务 UUID (16-bit)
  uint8_t characteristic_uuid1[2]; // 特征 UUID (16-bit)
  uint8_t characteristic_uuid2[2]; // 特征 UUID (16-bit)
} BleQueryConfigureUUIDResponse;

#pragma pack(pop)
// 蓝牙报文类型
typedef struct {
  uint8_t ble_frame_type;
  // A :0x11 请求参数报文 0x91:响应请求报文
  // B :0x12 获取数据报文 0x92:获取应答报文
  // C :0x13 设置参数报文 0x93:设置应答报文

} BLE_FrameType;

typedef struct {
  uint8_t ble_frame_header1;    // 蓝牙响应的帧头1
  uint8_t ble_frame_addr;       // 蓝牙响应的地址
  uint8_t ble_frame_header2;    // 蓝牙响应的帧头2
  BLE_FrameType ble_frame_type; // 蓝牙响应的控制码
  uint8_t ble_frame_label_L;    // 蓝牙响应的数据标识低字节
  uint8_t ble_frame_label_H;    // 蓝牙响应的数据标识高字节
  // 可变数据域
  uint8_t ble_frame_data[256]; // 蓝牙响应的数据域
  uint8_t ble_checksum;        // 蓝牙响应的校验和
  uint8_t ble_frame_tail;      // 蓝牙响应的帧尾
} BLE_ResponseFrame;

/*============================================================================
 *                          回调函数类型定义
 *===========================================================================*/

/**
 * @brief  蓝牙协议事件回调函数类型定义
 * @param event 协议事件
 */
typedef void (*BleEventCallback)(const BleProtocolEvent *event);

/*============================================================================
 *                          API函数声明
 *===========================================================================*/

#include "protocol_def.h" /* ProtocolInterface, ProtocolSendFunc */

/** 设备侧协议接口实例 (注册到 ProtocolManager 的 Device 槽) */
extern const ProtocolInterface diaphragm_gas_meter_protocol;

/**
 * @brief 设置蓝牙协议事件回调
 * @param callback 回调函数
 */
void DGM_SetEventCallback(BleEventCallback callback);

/**
 * @brief 获取事件类型名称 (用于调试日志)
 * @param type 事件类型
 * @return 事件名称字符串
 */
const char *DGM_GetEventName(BLE_EventType type);

/*---------- DS809 模组控制 API ----------*/

/** 读取当前连接状态 (发 0x12/0xB00E 给模组) */
void BLE_DS809_ReadConnectionStatus(void);

/** 读取模组固件版本 (发 0x12/0xB011 给模组) */
void BLE_DS809_ReadFirmwareVersion(void);

/** 读取蓝牙 MAC 地址 (发 0x12/0xC001 给模组) */
void BLE_DS809_ReadMacAddress(void);

/**
 * @brief 设置广播参数 (发 0x13/0xB012 给模组)
 * @param mode        0x00=停止, 0x01=慢速, 0x02=快速
 * @param interval_ms 快速广播间隔(ms), mode=0x02 时有效
 */
void BLE_DS809_SetBroadcastParams(uint8_t mode, uint16_t interval_ms);

/**
 * @brief 设置发射功率 (发 0x13/0xB013 给模组)
 * @param power  0x00=最小, 0x01=中等, 0x02=最大(+4dBm)
 */
void BLE_DS809_SetEmitPower(uint8_t power);

/**
 * @brief 设置蓝牙系统状态 (发 0x13/0xB000 给模组)
 * @param status  控制字节 (复位/低功耗等)
 */
void BLE_DS809_SetSystemStatus(uint8_t status);

/**
 * @brief 响应模组上电后的工作参数请求 (发 0x91/0xB007 给模组)
 *
 * 必须在收到 BLE_EVENT_CONFIGURE_BLUETOOTH_EVENT 事件后调用。
 */
void BLE_DS809_ReplyWorkParam(const BleQueryConfigureBluetoothResponse *config);

/**
 * @brief 响应模组上电后的 UUID 参数请求 (发 0x91/0xB00D 给模组)
 *
 * 传 NULL/0 则使用模组默认 UUID。
 */
void BLE_DS809_ReplyUUIDParam(const uint8_t *uuid_data, uint16_t len);

#endif /* __DEVICE_PROTOCOL_DIAPHRAGM_GAS_METER_EVENTS_H__ */
#endif /* __BT_DS809_H__ */
