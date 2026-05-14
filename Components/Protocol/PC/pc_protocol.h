/**
 * @file pc_protocol.h
 * @brief 上位机协议接口定义
 * @version 1.0.0
 * @date 2024-12
 *
 * @section intro 简介
 * 定义上位机协议的公共接口和命令码。
 * 所有上位机协议实现都应该包含此头文件。
 */

#ifndef __PC_PROTOCOL_H__
#define __PC_PROTOCOL_H__

#include "../protocol_def.h"
#include "utility.h"

/*============ PC协议命令码定义 ============*/

/**
 * @brief PC协议命令码枚举
 *
 * 定义上位机与测试工装之间的通信命令
 */
typedef enum {
  // 测试控制命令
  PC_CMD_START_TEST = 0x05,      // 开始测试
  PC_CMD_START_TEST_ACK = 0x85,  // 开始测试应答
  PC_CMD_QUERY_RESULT = 0x01,    // 查询测试结果
  PC_CMD_RESULT_RESPONSE = 0x81, // 测试结果响应

  // 配置命令
  PC_CMD_SET_CONFIG = 0xAE,     // 设置配置(日志/透传模式)
  PC_CMD_SET_CONFIG_ACK = 0xAF, // 设置配置应答

  // 读取命令
  PC_CMD_QUERY_CONFIG =
      0xC0, // 查询命令，数据域暂时只有程序版本，编译时间，等等
  PC_CMD_QUERY_CONFIG_ACK = 0xC1, // 查询命令应答
  // 控制命令
  PC_CMD_FT_CONTROL = 0xC2,     // 控制工装功能指令
  PC_CMD_FT_CONTROL_ACK = 0xC3, // 控制工装功能应答
  // 升级命令
  PC_CMD_UPGRADE = 0xBA,     // 升级命令
  PC_CMD_UPGRADE_ACK = 0xBB, // 升级应答

  // 诊断命令 (0xD0-0xDF)
  PC_CMD_FLASH_INFO = 0xD0,     // 查询Flash分区信息
  PC_CMD_FLASH_INFO_ACK = 0xD1, // Flash信息应答
  PC_CMD_FLASH_READ = 0xD2,     // 读取Flash数据
  PC_CMD_FLASH_READ_ACK = 0xD3, // 读取应答
  PC_CMD_TEST_STATS = 0xD4,     // 查询测试统计
  PC_CMD_TEST_STATS_ACK = 0xD5, // 测试统计应答

  // 控制工装的功能指令，比如检测低功耗，检测运行功耗，检测某个端口电压，开关膜表的电源等

  // 调试命令 (0xBE-0xBF)
  PC_CMD_QUERY_FAIL_STEP = 0xBE,     // 查询失败步骤
  PC_CMD_QUERY_FAIL_STEP_ACK = 0xBF, // 失败步骤应答

  // 其他命令
  PC_CMD_HEARTBEAT = 0xC0, // 心跳
  PC_CMD_RESET = 0xC1,     // 复位

  // 蓝牙透传命令
  PC_CMD_BT_FORWARD = 0x10,        // PC->MCU 将数据透传给蓝牙模块
  PC_CMD_QUERY_BLE_VERSION = 0xD1, // 查询蓝牙固件版本
  PC_CMD_QUERY_BLE_STATUS = 0xD2,  // 查询蓝牙状态

} PCProtocolCmd;

/*============ PC协议数据结构 ============*/

/**
 * @brief 开始测试命令参数
 */
typedef struct {
  uint8_t station_id;      // 工位号
  uint8_t meter_number[6]; // 表具编号
  uint8_t meter_type;      // 表具类型: 0=机械表, 1=超声波表

  // 超声波表参数
  uint8_t ultrasonic_meter_type; // 超声波表计类型
  uint8_t ultrasonic_transducer; // 换能器类型
  uint16_t ultrasonic_pipe_type; // 管道类型
  uint8_t ultrasonic_valve_type; // 阀门类型
  uint8_t module_type;           // 模块类型: 0=NB, 1=CAT1

  // 机械表参数
  uint16_t mechanical_pipe_mm;   // 管径(mm)
  uint8_t mechanical_valve_type; // 阀门类型
  uint8_t mechanical_timeout;    // 超时时间(秒)
  uint16_t mechanical_stall_ma;  // 堵转电流(mA)
} PCStartTestParam;

/**
 * @brief 配置命令参数
 */
typedef struct {
  uint8_t station_id;       // 工位号
  uint8_t debug_mode;       // 调试模式: 0=关, 1=开
  uint8_t passthrough_mode; // 透传模式: 0=关, 1=开
} PCConfigParam;

/**
 * @brief 升级命令参数
 */
typedef struct {
  uint8_t station_id;  // 工位号
  uint8_t mode;        // 模式: 0=手动, 1=自动
  uint8_t baud_config; // 波特率: 0=9600, 1=115200
  uint8_t protocol;    // 协议: 0=Xmodem
  uint8_t timeout_sec; // 超时时间(秒)
  uint8_t log_enable;  // 日志: 0=关, 1=开
  uint16_t fw_size_kb; // 固件大小(KB)
} PCUpgradeParam;

/*============ 回调函数类型定义 ============*/

/**
 * @brief 获取本机工位号回调函数类型
 * @return 本机工位号 (1-4)
 * @note 用于协议层与测试模块解耦
 */
typedef uint8_t (*PCGetStationIdFunc)(void);

/**
 * @brief 获取测试失败信息回调函数类型
 * @param[out] step_id     失败步骤ID (枚举值)
 * @param[out] step_name   失败步骤名称 (ASCII字符串，最大64字节)
 * @param[out] fail_reason 失败原因代码 (枚举值，0=无失败)
 * @param[out] reason_name 失败原因名称 (ASCII字符串，最大64字节)
 * @return 测试状态: 0=进行中, 1=成功, 2=失败
 * @note 用于协议层查询测试失败详情
 */
typedef uint8_t (*PCGetFailInfoFunc)(uint8_t *step_id, char *step_name,
                                     uint8_t *fail_reason, char *reason_name);

/**
 * @brief 设置获取工位号的回调函数
 * @param func 回调函数指针
 * @note 应在协议初始化后、使用前调用
 */
void PC_Protocol_SetStationIdFunc(PCGetStationIdFunc func);

/**
 * @brief 获取本机工位号
 * @return 本机工位号，如果未设置回调则返回0
 */
uint8_t PC_Protocol_GetStationId(void);

/**
 * @brief 设置获取失败信息的回调函数
 * @param func 回调函数指针
 */
void PC_Protocol_SetFailInfoFunc(PCGetFailInfoFunc func);

/**
 * @brief 获取测试失败信息
 * @param[out] step_id     失败步骤ID
 * @param[out] step_name   失败步骤名称
 * @param[out] fail_reason 失败原因代码
 * @param[out] reason_name 失败原因名称
 * @return 测试状态: 0=进行中, 1=成功, 2=失败
 */
uint8_t PC_Protocol_GetFailInfo(uint8_t *step_id, char *step_name,
                                uint8_t *fail_reason, char *reason_name);

/**
 * @brief 获取程序版本号的回调函数类型
 * @return 版本号 (高字节=主版本, 低字节=次版本)
 */
typedef uint16_t (*PCGetVersionFunc)(void);

/**
 * @brief 获取编译时间的回调函数类型
 * @param[out] build_time 编译时间字符串缓冲区
 */
typedef void (*PCGetBuildTimeFunc)(char *build_time);

/**
 * @brief 设置获取程序版本号的回调函数
 * @param func 回调函数指针
 * @return 设置后的回调函数指针
 */
PCGetVersionFunc PC_Protocol_SetGetVersionFunc(PCGetVersionFunc func);

/**
 * @brief  控制工装功能的回调函数类型
 * @param data 完整帧数据
 * @param length 数据长度
 */
typedef void (*PCFTControlFunc)(const uint8_t *data, uint16_t length);

PCFTControlFunc PC_Protocol_SetFTControlFunc(PCFTControlFunc func);
PCFTControlFunc PC_Protocol_GetFTControlFunc(void);

/**
 * @brief 设置获取编译时间的回调函数
 * @param func 回调函数指针
 * @return 设置后的回调函数指针
 */
PCGetBuildTimeFunc PC_Protocol_SetGetBuildTimeFunc(PCGetBuildTimeFunc func);

/*============ 协议实例声明 ============*/

/**
 * @brief 国内水表PC协议实例
 *
 * 在 PC/Domestic/WaterMeters/pc_protocol_water_meter.c 中定义
 * 协议格式: 68 CMD LEN DATA... CHECKSUM 16
 */
extern const ProtocolInterface water_meter_pc_protocol;

/**
 * @brief 国内膜式燃气表PC协议实例
 *
 * 在 PC/Domestic/DiaphragmGasMeters/pc_protocol_diaphragm_gas_meter.c 中定义
 * 使用双68帧格式: 68 [表号6字节] 68 [控制码] [长度2字节] ...
 */
extern const ProtocolInterface diaphragm_gas_meter_pc_protocol;

/**
 * @brief Legacy协议适配层实例
 *
 * 在 pc_protocol_legacy.c 中定义
 * 用于对接现有的 PC_xieyi_Ctrl.c 代码
 */
extern const ProtocolInterface legacy_pc_protocol;

/**
 * @brief APP升级协议实例
 *
 * 在 pc_protocol_upgrade.c 中定义
 * 处理固件升级相关命令 (0xBA/0xBB)
 */
extern const ProtocolInterface upgrade_pc_protocol;

/**
 * @brief 公共调试配置协议实例
 *
 * 在 pc_protocol_config.c 中定义
 * 处理调试配置命令 (0xAE/0xAF) 和查询步骤 (0xBE/0xBF)
 * 此协议与具体表计类型无关，所有表计都可使用
 */
extern const ProtocolInterface config_pc_protocol;

/*============ 调试配置协议公共API ============*/

/**
 * @brief 获取当前调试模式状态
 * @return 0=关闭, 1=开启
 */
uint8_t PC_Config_GetDebugMode(void);

/**
 * @brief 获取当前透传模式状态
 * @return 0=关闭, 1=开启
 */
uint8_t PC_Config_GetPassThroughMode(void);

/**
 * @brief 设置调试模式
 * @param mode 0=关闭, 1=开启
 */
void PC_Config_SetDebugMode(uint8_t mode);

/**
 * @brief 设置透传模式
 * @param mode 0=关闭, 1=开启
 */
void PC_Config_SetPassThroughMode(uint8_t mode);

/*============ 升级协议公共API ============*/

/**
 * @brief 检查是否有待处理的升级请求
 * @return true=有升级请求待处理
 */
bool Upgrade_IsPending(void);

/**
 * @brief 获取待处理的升级参数
 * @param mode 升级模式 (0=手动, 1=自动)
 * @param baud_cfg 波特率配置 (0=9600, 1=115200)
 * @param timeout_sec 超时时间(秒)
 * @param fw_size_kb 固件大小(KB)
 * @return true=成功获取
 */
bool Upgrade_GetPendingParams(uint8_t *mode, uint8_t *baud_cfg,
                              uint8_t *timeout_sec, uint16_t *fw_size_kb);

/**
 * @brief 清除升级请求
 */
void Upgrade_ClearPending(void);

/*============ MES协议公共API ============*/

/**
 * @brief 发送测试结果到PC端
 *
 * 替代原来 PC_xieyi_Ctrl.c 中的 PC_test_result_analysis()
 * 在测试完成时调用此函数发送结果
 */
void PC_SendTestResult(void);

/**
 * @brief 调试模式下分析并打印测试结果
 *
 * 兼容原 PC_test_result_analysis() 函数名
 */
void PC_test_result_analysis(void);

/*============ 辅助函数 ============*/

/**
 * @brief 计算累加和校验 (兼容接口，调用 util_checksum_sum8)
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 校验和
 */
static inline uint8_t pc_calc_checksum(const uint8_t *data, uint16_t len) {
  return util_checksum_sum8(data, len);
}

#endif /* __PC_PROTOCOL_H__ */
