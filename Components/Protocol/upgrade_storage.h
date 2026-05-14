/**
 * @file upgrade_storage.h
 * @brief 升级参数Flash存储接口
 * @version 1.0.0
 * @date 2026-01-05
 *
 * 提供升级参数的持久化存储功能，使用独立的Flash分区
 * 支持APP和Bootloader共享访问
 */

#ifndef __UPGRADE_STORAGE_H__
#define __UPGRADE_STORAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*============================================================================
 * 存储结构定义
 *===========================================================================*/

/**
 * @brief 升级参数存储格式 (v2.0)
 *
 * 该结构体存储在Flash的upgrade_params分区中
 * Bootloader读取此结构获取升级配置信息
 */
#pragma pack(1)
typedef struct {
  uint32_t magic;       /**< 魔数标识: 0x55AA55AA 表示有效数据 */
  uint8_t version;      /**< 结构体版本号 */
  uint8_t station_id;   /**< 工位号 (0-7) */
  uint8_t upgrade_mode; /**< 升级模式: 0=手动, 1=自动 */
  uint8_t baud_config;  /**< 波特率配置: 0=9600, 1=115200 */
  uint8_t protocol;     /**< 传输协议: 0=Xmodem */
  uint8_t timeout_sec;  /**< 超时时间(秒) */
  uint8_t log_enable;   /**< Bootloader日志: 0=关, 1=开 */
  uint8_t reserved;     /**< 保留字节 */
  uint16_t fw_size_kb;  /**< 固件大小(KB) */
  uint16_t chip_code;   /**< 芯片代码 (与魔数中的chip_code一致) */
  uint8_t vendor_code;  /**< 厂商代码 */
  uint8_t upgrade_flag; /**< 升级标志: 0=正常启动, 1=进入升级模式 */
  uint32_t checksum;    /**< CRC32校验 */
} UpgradeStorageData_t;
#pragma pack()

/* 魔数定义 */
#define UPGRADE_STORAGE_MAGIC 0x55AA55AA
#define UPGRADE_STORAGE_VERSION 0x02

/* 升级标志 */
#define UPGRADE_FLAG_NORMAL 0x00  /**< 正常启动 */
#define UPGRADE_FLAG_UPGRADE 0x01 /**< 进入升级模式 */

/*============================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化升级参数存储模块
 * @return true: 成功, false: 失败
 */
bool UpgradeStorage_Init(void);

/**
 * @brief 保存升级参数到Flash
 *
 * @param station_id 工位号
 * @param mode 升级模式 (0=手动, 1=自动)
 * @param baud_cfg 波特率配置 (0=9600, 1=115200)
 * @param protocol 传输协议 (0=Xmodem)
 * @param timeout 超时时间(秒)
 * @param log_en 日志开关
 * @param fw_size_kb 固件大小(KB)
 * @param vendor 厂商代码
 * @param chip 芯片代码
 * @return true: 成功, false: 失败
 */
bool UpgradeStorage_SaveParams(uint8_t station_id, uint8_t mode,
                               uint8_t baud_cfg, uint8_t protocol,
                               uint8_t timeout, uint8_t log_en,
                               uint16_t fw_size_kb, uint8_t vendor,
                               uint16_t chip);

/**
 * @brief 读取升级参数
 *
 * @param data 输出参数结构体指针
 * @return true: 有效数据, false: 无数据或数据无效
 */
bool UpgradeStorage_ReadParams(UpgradeStorageData_t *data);

/**
 * @brief 设置升级标志
 *
 * 设置后下次启动Bootloader会检测到升级请求
 *
 * @param flag UPGRADE_FLAG_NORMAL 或 UPGRADE_FLAG_UPGRADE
 * @return true: 成功, false: 失败
 */
bool UpgradeStorage_SetUpgradeFlag(uint8_t flag);

/**
 * @brief 获取升级标志
 * @return 当前升级标志值，无效数据返回 UPGRADE_FLAG_NORMAL
 */
uint8_t UpgradeStorage_GetUpgradeFlag(void);

/**
 * @brief 清除升级参数
 *
 * 擦除Flash分区，清除所有存储的参数
 *
 * @return true: 成功, false: 失败
 */
bool UpgradeStorage_Clear(void);

/**
 * @brief 检查是否有待处理的升级请求
 * @return true: 有升级请求, false: 无
 */
bool UpgradeStorage_HasPendingUpgrade(void);

#ifdef __cplusplus
}
#endif

#endif /* __UPGRADE_STORAGE_H__ */
