/**
 * @file system_reset.c
 * @brief 系统复位相关函数实现
 * @version 1.0.0
 * @date 2026-01-05
 *
 * 实现系统复位进入Bootloader的功能
 */

#define LOG_TAG "sys_reset"

#include "fm33le0xx_fl.h"
#include <elog.h>
#include <stdint.h>

/*============================================================================
 * 外部依赖
 *===========================================================================*/

/* 升级存储模块 */
#ifdef COMPONENT_UPGRADE_STORAGE
#include "Protocol/upgrade_storage.h"
#endif

/*============================================================================
 * 系统复位实现
 *===========================================================================*/

/**
 * @brief 软件复位进入Bootloader
 *
 * 此函数会:
 * 1. 确保升级参数已保存到Flash
 * 2. 执行软件复位
 * 3. Bootloader启动后会检测升级标志并进入升级模式
 *
 * @note 这个函数实现了 pc_protocol_upgrade.c 中声明的弱符号
 */
void system_reset_to_bootloader(void) {
  log_i("========================================");
  log_i("         即将重启进入Bootloader");
  log_i("========================================");

  /* 给串口一些时间发送完日志 */
  for (volatile uint32_t i = 0; i < 100000; i++) {
    __NOP();
  }

  /* 禁用所有中断 */
  __disable_irq();

  /* 执行系统复位 */
  log_i("执行系统复位...");

  /* 使用 CMSIS 提供的复位函数 */
  NVIC_SystemReset();

  /* 永远不会执行到这里 */
  while (1) {
    __NOP();
  }
}

/**
 * @brief 检查是否需要进入Bootloader升级模式
 *
 * 在APP启动时调用，检查Flash中是否有升级请求
 * 如果有，则跳转到Bootloader
 *
 * @return true: 需要升级, false: 正常启动
 */
bool System_CheckUpgradeRequest(void) {
#ifdef COMPONENT_UPGRADE_STORAGE
  /* 初始化升级存储 (如果还没初始化) */
  if (!UpgradeStorage_Init()) {
    return false;
  }

  /* 检查是否有待处理的升级请求 */
  if (UpgradeStorage_HasPendingUpgrade()) {
    log_i("检测到升级请求，准备进入Bootloader...");
    return true;
  }
#endif
  return false;
}

/**
 * @brief 清除升级请求
 *
 * 在升级完成后或APP正常启动后调用
 */
void System_ClearUpgradeRequest(void) {
#ifdef COMPONENT_UPGRADE_STORAGE
  UpgradeStorage_SetUpgradeFlag(UPGRADE_FLAG_NORMAL);
  log_i("升级标志已清除");
#endif
}
