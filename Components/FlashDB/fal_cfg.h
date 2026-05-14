/*
 * @file fal_cfg.h
 * @brief FAL (Flash Abstraction Layer) 配置文件 - FM33LE015
 *
 * 与 fal_flash_fm33le015_port.c 中导出的 fm33le015_onchip_flash 设备配套。
 */

#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

#include <elog.h>

/*============================================================================
 * FAL 调试输出
 *===========================================================================*/
#define FAL_PRINTF(...) elog_raw_output(__VA_ARGS__)

/*============================================================================
 * Flash 设备定义
 *===========================================================================*/

#define FM33LE015_FLASH_DEV_NAME "fm33le015_onchip"

/* 声明 Flash 设备 */
extern const struct fal_flash_dev fm33le015_onchip_flash;

/* Flash 设备表 */
#define FAL_FLASH_DEV_TABLE                                                    \
  { &fm33le015_onchip_flash, }

/*============================================================================
 * 分区表配置
 *===========================================================================*/

/* 启用分区表配置 */
#define FAL_PART_HAS_TABLE_CFG

/*
 * 分区表说明 (FM33LE015 64KB Flash):
 * - test_stats     分区用于存储测试统计信息 (支持磨损均衡)
 * - upgrade_params 分区用于存储升级参数，Bootloader 和 APP 共享
 * - kvdb           分区用于 FlashDB 的 KVDB 存储
 */
#define FAL_PART_TABLE                                                         \
  {                                                                            \
    {FAL_PART_MAGIC_WORD,                                                      \
     "test_stats",                                                             \
     FM33LE015_FLASH_DEV_NAME,                                                 \
     0x3C000,                                                                  \
     8 * 1024,                                                                 \
     0},                                                                       \
        {FAL_PART_MAGIC_WORD,                                                  \
         "upgrade_params",                                                     \
         FM33LE015_FLASH_DEV_NAME,                                             \
         0x3E000,                                                              \
         4 * 1024,                                                             \
         0},                                                                   \
        {FAL_PART_MAGIC_WORD,                                                  \
         "kvdb",                                                               \
         FM33LE015_FLASH_DEV_NAME,                                             \
         0x3F000,                                                              \
         4 * 1024,                                                             \
         0},                                                                   \
  }

#endif /* _FAL_CFG_H_ */
