/**
 * @file flash_diag.h
 * @brief Flash诊断和分区信息查询接口
 * @version 1.0.0
 * @date 2026-01-05
 *
 * 提供Flash分区信息查询、验证和诊断功能
 * 支持通过PC协议远程查询Flash状态
 */

#ifndef __FLASH_DIAG_H__
#define __FLASH_DIAG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*============================================================================
 * Flash 分区地址定义
 *===========================================================================*/

/* Flash 基地址 */
#define FLASH_BASE_ADDR 0x00000000UL
#define FLASH_TOTAL_SIZE (256 * 1024) /* 256KB */

/* 分区地址 */
#define FLASH_BOOTLOADER_ADDR 0x00000000UL
#define FLASH_BOOTLOADER_SIZE (16 * 1024) /* 16KB */

#define FLASH_APP_ADDR 0x00004000UL
#define FLASH_APP_SIZE (224 * 1024) /* 224KB */

#define FLASH_TEST_STATS_ADDR 0x0003C000UL
#define FLASH_TEST_STATS_SIZE (8 * 1024) /* 8KB */

#define FLASH_UPGRADE_ADDR 0x0003E000UL
#define FLASH_UPGRADE_SIZE (4 * 1024) /* 4KB */

#define FLASH_KVDB_ADDR 0x0003F000UL
#define FLASH_KVDB_SIZE (4 * 1024) /* 4KB */

/* 扇区大小 */
#define FLASH_SECTOR_SIZE (2 * 1024) /* 2KB */

/*============================================================================
 * 分区信息结构体
 *===========================================================================*/

/**
 * @brief 分区信息
 */
typedef struct {
  const char *name; /**< 分区名称 */
  uint32_t addr;    /**< 起始地址 */
  uint32_t size;    /**< 分区大小 */
  uint32_t used;    /**< 已用空间 (如果适用) */
  uint8_t valid;    /**< 数据是否有效 */
} FlashPartitionInfo_t;

/**
 * @brief Flash诊断信息
 */
typedef struct {
  uint32_t total_size;                /**< Flash总大小 */
  uint32_t sector_size;               /**< 扇区大小 */
  uint8_t partition_count;            /**< 分区数量 */
  FlashPartitionInfo_t partitions[5]; /**< 分区信息数组 */
} FlashDiagInfo_t;

/*============================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化Flash诊断模块
 * @return true: 成功, false: 失败
 */
bool FlashDiag_Init(void);

/**
 * @brief 获取Flash诊断信息
 * @param info 输出诊断信息结构体指针
 * @return true: 成功, false: 失败
 */
bool FlashDiag_GetInfo(FlashDiagInfo_t *info);

/**
 * @brief 打印Flash分区信息到日志
 */
void FlashDiag_PrintInfo(void);

/**
 * @brief 读取指定分区的原始数据
 * @param partition_name 分区名称
 * @param offset 分区内偏移
 * @param buf 输出缓冲区
 * @param size 读取大小
 * @return 实际读取的字节数，-1表示错误
 */
int FlashDiag_ReadPartition(const char *partition_name, uint32_t offset,
                            uint8_t *buf, uint32_t size);

/**
 * @brief 验证分区数据完整性
 * @param partition_name 分区名称
 * @return true: 数据有效, false: 数据无效或空
 */
bool FlashDiag_ValidatePartition(const char *partition_name);

/**
 * @brief 擦除指定分区
 * @param partition_name 分区名称
 * @return true: 成功, false: 失败
 */
bool FlashDiag_ErasePartition(const char *partition_name);

/**
 * @brief 直接读取Flash地址 (调试用)
 * @param addr Flash地址
 * @param buf 输出缓冲区
 * @param size 读取大小
 * @return 实际读取的字节数
 */
int FlashDiag_ReadRaw(uint32_t addr, uint8_t *buf, uint32_t size);

/**
 * @brief 打印指定地址的Flash内容 (HEX格式)
 * @param addr 起始地址
 * @param size 大小 (最大256字节)
 */
void FlashDiag_DumpHex(uint32_t addr, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_DIAG_H__ */
