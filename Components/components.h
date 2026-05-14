/**
 * @file components.h
 * @brief 组件统一初始化接口
 * @version 1.0.0
 * @date 2026-01-05
 *
 * @section intro 简介
 * 提供统一的组件初始化入口，通过宏定义启用/禁用各个模块。
 * 移植时只需修改本文件的宏配置，无需修改 main.c。
 *
 * @section usage 使用方法
 * 1. 在本文件中启用需要的组件宏
 * 2. 在 main.c 中调用 Components_Init()
 * 3. 如需设置发送函数，调用 Components_SetSendFunctions()
 *
 * @code
 * // main.c 中的使用示例
 * #include "components.h"
 *
 * void system_init(void) {
 *     // ... 其他初始化 ...
 *     Components_Init();
 *     Components_SetSendFunctions(Uart0_Tx_Send, Uart1_Tx_Send_ok);
 * }
 * @endcode
 */

#ifndef __COMPONENTS_H__
#define __COMPONENTS_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 组件启用配置 (根据项目需求修改)
 *===========================================================================*/

/**
 * @defgroup COMPONENT_CONFIG 组件配置
 * @brief 通过定义/注释这些宏来启用/禁用组件
 * @{
 */

/** @brief 启用协议管理器 (基础组件，通常必须启用) */
#define COMPONENT_PROTOCOL_MANAGER

/** @brief 启用国内水表PC协议 (PC端MES系统通信) */
// #define COMPONENT_PC_PROTOCOL_WATER_METER

/** @brief 启用国内膜式燃气表PC协议 (PC端MES系统通信) */
// #define COMPONENT_PC_PROTOCOL_DIAPHRAGM_GAS_METER

/** @brief 启用APP升级协议 (固件升级功能) */
#define COMPONENT_PROTOCOL_UPGRADE

/** @brief 启用公共调试配置协议 (调试模式/透传模式/查询步骤) */
#define COMPONENT_PC_PROTOCOL_CONFIG

/** @brief 启用蓝牙通讯协议 (设备侧 DS809 模组协议)
 *  @note  对应 Components/Protocol/App/BT_Passthrough/bt_ds809.c，
 *         由 App/ble.c::BLE_INIT() 注册到 Device 槽，无需此宏。 */
// #define COMPONENT_PC_PROTOCOL_BT_PASSTHROUGH

/** @brief 启用国内水表设备协议 (设备端水表通信) */
// #define COMPONENT_DEVICE_PROTOCOL_WATER_METER

/** @brief 启用国内膜式燃气表设备协议 (设备端膜式燃气表通信) */
// #define COMPONENT_DEVICE_PROTOCOL_DIAPHRAGM_GAS_METER

/** @brief 启用Legacy协议 (兼容旧版 PC_xieyi_Ctrl.c) */
// #define COMPONENT_PROTOCOL_LEGACY

/** @brief 启用EasyLogger日志组件 */
#define COMPONENT_EASYLOGGER

/** @brief 启用FlashDB数据库组件 */
#define COMPONENT_FLASHDB

/** @brief 启用升级参数存储 */
#define COMPONENT_UPGRADE_STORAGE

/** @} */

/*============================================================================
 * 默认活跃协议配置
 *===========================================================================*/

/**
 * @brief 默认PC协议名称
 * @note 可选: "mes", "upgrade", "legacy"
 *       如果不定义，则使用第一个注册的协议
 */
#define COMPONENT_DEFAULT_PC_PROTOCOL "mes"

/**
 * @brief 默认设备协议名称
 * @note 可选: "water_meter"
 *       如果不定义，则使用第一个注册的协议
 */
#define COMPONENT_DEFAULT_DEVICE_PROTOCOL "water_meter"

/*============================================================================
 * 组件头文件包含
 *===========================================================================*/

#ifdef COMPONENT_PROTOCOL_MANAGER
#include "Protocol/protocol.h"
#endif

#ifdef COMPONENT_EASYLOGGER
#include "EasyLogger/easylogger/inc/elog.h"
#endif

#ifdef COMPONENT_FLASHDB
#include "FlashDB/flash_diag.h"
#include "FlashDB/inc/flashdb.h"
#include "FlashDB/port/fal/inc/fal.h"
#include "FlashDB/test_stats.h"
#endif

/*============================================================================
 * 修复 fal_def.h 对 EasyLogger 日志宏的覆盖
 *===========================================================================*/
/**
 * fal_def.h 会 #undef log_i/log_e/log_d 并重定义为 FAL_PRINTF (默认=printf)。
 * 这导致所有 #include "components.h" 之后的文件中，log_i 等宏不再调用
 * elog_output，而是调用 printf。在 Keil MicroLib 下 printf 走 semihosting，
 * 无调试器时触发 HardFault 导致系统崩溃。
 *
 * 此处恢复 EasyLogger 的 log_* 宏定义 (与 elog.h 中的定义一致)。
 */
#if defined(COMPONENT_EASYLOGGER) && defined(ELOG_OUTPUT_ENABLE)
/* 撤销 fal_def.h 的覆盖 */
#ifdef log_d
#undef log_d
#endif
#ifdef log_e
#undef log_e
#endif
#ifdef log_i
#undef log_i
#endif

/* 恢复 EasyLogger 版本 (需要调用方在 #include 前定义 LOG_TAG) */
#if ELOG_OUTPUT_LVL >= ELOG_LVL_DEBUG
#define log_d(...) elog_d(LOG_TAG, __VA_ARGS__)
#else
#define log_d(...) ((void)0)
#endif
#if ELOG_OUTPUT_LVL >= ELOG_LVL_ERROR
#define log_e(...) elog_e(LOG_TAG, __VA_ARGS__)
#else
#define log_e(...) ((void)0)
#endif
#if ELOG_OUTPUT_LVL >= ELOG_LVL_INFO
#define log_i(...) elog_i(LOG_TAG, __VA_ARGS__)
#else
#define log_i(...) ((void)0)
#endif
#endif /* COMPONENT_EASYLOGGER && ELOG_OUTPUT_ENABLE */

#ifdef COMPONENT_UPGRADE_STORAGE
#include "Protocol/upgrade_storage.h"
#endif

/*============================================================================
 * 组件初始化API
 *===========================================================================*/

/**
 * @brief 组件初始化配置结构体
 */
typedef struct {
  void (*pc_send)(uint8_t *, uint16_t);        ///< PC端串口发送函数
  void (*bluetooth_send)(uint8_t *, uint16_t); ///< 蓝牙发送函数
  uint16_t (*get_version)(void);               ///< 获取程序版本号回调
  void (*get_build_time)(char *);              ///< 获取编译时间回调
  void (*ft_control)(const uint8_t *,
                     uint16_t); ///< 工装控制回调
} ComponentsConfig;

/**
 * @brief 初始化所有启用的组件
 *
 * 根据宏配置自动初始化各个组件，包括：
 * - 协议管理器
 * - 各种协议注册
 * - 阀门控制
 * - 日志系统
 * - 数据库
 *
 * @param config 配置结构体指针，包含所有回调函数
 * @return true 初始化成功
 * @return false 初始化失败
 *
 * @code
 * // main.c 中的使用示例
 * ComponentsConfig cfg = {
 *     .pc_send = Uart0_Tx_Send,
 *     .device_send = Uart1_Tx_Send_ok,
 *     .get_station_id = Callback_GetStationId,
 *     .get_fail_info = Callback_GetFailInfo,
 *     .get_version = Callback_GetVersion,
 *     .get_build_time = Callback_GetBuildTime,
 * };
 * Components_Init(&cfg);
 * @endcode
 */
bool Components_Init(const ComponentsConfig *config);

/**
 * @brief 设置通信发送函数
 *
 * @param pc_send PC端串口发送函数 (如 Uart0_Tx_Send)
 * @param device_send 设备端串口发送函数 (如 Uart1_Tx_Send_ok)
 */
void Components_SetSendFunctions(void (*pc_send)(uint8_t *, uint16_t),
                                 void (*device_send)(uint8_t *, uint16_t));

/**
 * @brief 设置工位号获取回调函数
 *
 * 用于协议层与测试模块解耦，协议层通过此回调获取本机工位号。
 *
 * @param get_station_id 获取工位号的函数指针
 *
 * @code
 * // main.c 中的使用示例
 * uint8_t get_local_station_id(void) {
 *     return Test_jiejuo_jilu.gongwei;
 * }
 *
 * void system_init(void) {
 *     Components_Init();
 *     Components_SetStationIdFunc(get_local_station_id);
 * }
 * @endcode
 */
void Components_SetStationIdFunc(uint8_t (*get_station_id)(void));

/**
 * @brief 设置测试失败信息获取回调函数
 *
 * 用于协议层查询测试失败详情。
 *
 * @param get_fail_info 获取失败信息的函数指针 (step_id, step_name, fail_reason,
 * reason_name) -> status
 * @note 回调参数: (uint8_t *step_id, char *step_name, uint8_t *fail_reason,
 * char *reason_name)
 * @note 回调返回: 测试状态 0=进行中, 1=成功, 2=失败
 *
 * @code
 * // main.c 中的使用示例
 * uint8_t get_test_fail_info(uint8_t *step_id, char *step_name,
 *                            uint8_t *fail_reason, char *reason_name) {
 *     *step_id = diaphragm_runtime.fail_step;
 *     strcpy(step_name, diaphragm_step_table[*step_id].name);
 *     *fail_reason = diaphragm_runtime.fail_reason;
 *     strcpy(reason_name, diaphragm_get_fail_reason_string(*fail_reason));
 *     return test_status; // 0=进行中, 1=成功, 2=失败
 * }
 * @endcode
 */
void Components_SetFailInfoFunc(uint8_t (*get_fail_info)(uint8_t *, char *,
                                                         uint8_t *, char *));

/**
 * @brief 设置程序版本号获取回调函数
 *
 * @param get_version 回调函数指针，返回版本号 (高字节=主版本, 低字节=次版本)
 */
void Components_SetVersionFunc(uint16_t (*get_version)(void));

/**
 * @brief 设置编译时间获取回调函数
 *
 * @param get_build_time 回调函数指针，填充编译时间字符串
 */
void Components_SetBuildTimeFunc(void (*get_build_time)(char *));

/**
 * @brief 设置工装控制命令回调函数
 *
 * @param ft_control 回调函数指针，接收完整帧数据
 */
void Components_SetFTControlFunc(void (*ft_control)(const uint8_t *, uint16_t));

/**
 * @brief 获取组件版本信息
 *
 * @return 版本字符串
 */
const char *Components_GetVersion(void);

/**
 * @brief 打印组件配置信息
 *
 * 打印当前启用的组件列表
 */
void Components_PrintInfo(void);

#ifdef __cplusplus
}
#endif

#endif /* __COMPONENTS_H__ */
