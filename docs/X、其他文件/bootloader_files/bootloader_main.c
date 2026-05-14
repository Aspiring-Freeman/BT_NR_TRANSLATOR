/**
 ******************************************************************************
 * @file    bootloader_main.c
 * @brief   FM33LG0xx Bootloader 主程序
 * @author  SmallCaliberUWMTester
 * @version v1.0.0
 * @date    2025-12-23
 ******************************************************************************
 *
 * 功能:
 * ====
 * 1. 检查 APP 是否有效
 * 2. 跳转到 APP 执行
 *
 * 注意:
 * ====
 * - 中断转发由 startup_fm33lg0xx_bootloader.s 实现
 * - Bootloader 的向量表前两项 (SP, Reset) 是自己的
 * - 其他所有中断都转发到 APP 的向量表
 *
 ******************************************************************************
 */

#include <stdint.h>

/* ============================================================================
 * 配置
 * ============================================================================
 */

#define APP_START_ADDRESS 0x00004000UL /* APP 起始地址 */
#define APP_END_ADDRESS 0x00040000UL   /* Flash 结束地址 */
#define SRAM_START 0x20000000UL        /* SRAM 起始地址 */
#define SRAM_END 0x20008000UL          /* SRAM 结束地址 (32KB) */

/* ============================================================================
 * 类型定义
 * ============================================================================
 */

typedef void (*pFunction)(void);

/* ============================================================================
 * 函数声明
 * ============================================================================
 */

static int Bootloader_CheckAppValid(void);
static void Bootloader_JumpToApp(void);

/* ============================================================================
 * 主函数
 * ============================================================================
 */

int main(void) {
  /* 可选: 在这里添加 Bootloader 功能
   * - 检查升级标志
   * - 通过 UART/USB 接收新固件
   * - 验证固件完整性 (CRC)
   */

  /* 检查 APP 是否有效 */
  if (Bootloader_CheckAppValid()) {
    /* 跳转到 APP */
    Bootloader_JumpToApp();
  }

  /* APP 无效，进入死循环或升级模式 */
  while (1) {
    /* TODO: 实现升级模式 */
  }
}

/* ============================================================================
 * 检查 APP 是否有效
 * ============================================================================
 *
 * 检查方法:
 * 1. APP 向量表第一个字 (初始 SP) 应指向有效的 SRAM 地址
 * 2. APP 向量表第二个字 (Reset_Handler) 应指向有效的 Flash 地址
 */
static int Bootloader_CheckAppValid(void) {
  uint32_t app_sp = *(volatile uint32_t *)APP_START_ADDRESS;
  uint32_t app_reset = *(volatile uint32_t *)(APP_START_ADDRESS + 4);

  /* 检查 SP 是否在 SRAM 范围内 */
  if (app_sp < SRAM_START || app_sp > SRAM_END) {
    return 0; /* 无效 */
  }

  /* 检查 Reset_Handler 是否在 APP Flash 范围内 */
  /* 注意: Thumb 指令地址最低位为 1 */
  uint32_t reset_addr = app_reset & 0xFFFFFFFE;
  if (reset_addr < APP_START_ADDRESS || reset_addr >= APP_END_ADDRESS) {
    return 0; /* 无效 */
  }

  return 1; /* 有效 */
}

/* ============================================================================
 * 跳转到 APP
 * ============================================================================
 *
 * 步骤:
 * 1. 获取 APP 的初始栈指针 (向量表第一个字)
 * 2. 获取 APP 的 Reset_Handler 地址 (向量表第二个字)
 * 3. 设置 MSP 为 APP 的初始栈指针
 * 4. 跳转到 APP 的 Reset_Handler
 *
 * 注意:
 * - 不需要重新配置中断，因为中断转发由启动文件实现
 * - APP 的 Reset_Handler 会调用 SystemInit 和 main
 */
static void Bootloader_JumpToApp(void) {
  uint32_t app_sp = *(volatile uint32_t *)APP_START_ADDRESS;
  uint32_t app_reset = *(volatile uint32_t *)(APP_START_ADDRESS + 4);

  pFunction JumpToApplication;

  /* 禁用所有中断 */
  __disable_irq();

  /* 设置 MSP 为 APP 的初始栈指针 */
  __set_MSP(app_sp);

  /* 获取 APP 的 Reset_Handler */
  JumpToApplication = (pFunction)app_reset;

  /* 跳转到 APP */
  JumpToApplication();

  /* 永远不会到达这里 */
  while (1)
    ;
}

/* ============================================================================
 * CMSIS 函数 (如果没有包含 CMSIS 头文件)
 * ============================================================================
 */

#ifndef __CORE_CM0PLUS_H_DEPENDANT

__attribute__((always_inline)) static inline void __disable_irq(void) {
  __asm volatile("cpsid i" : : : "memory");
}

__attribute__((always_inline)) static inline void
__set_MSP(uint32_t topOfMainStack) {
  __asm volatile("MSR msp, %0" : : "r"(topOfMainStack) :);
}

#endif
