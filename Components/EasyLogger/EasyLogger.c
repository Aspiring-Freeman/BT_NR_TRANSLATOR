/**
 * @file EasyLogger.c
 * @brief Keil-specific EasyLogger support (NOT compiled by GCC/CMake)
 *
 * 此文件仅在 Keil 工程中编译。提供以下功能:
 * 1. fputc 重定向 - 防止 MicroLib 默认的 semihosting 实现
 *    在无调试器连接时触发 HardFault (BKPT → HardFault on Cortex-M0)
 *
 * 背景: Keil MicroLib 的 printf/fputc 默认使用 semihosting，
 * 如果代码中有任何 printf 调用 (包括第三方库如 FlashDB/FAL)，
 * 在无调试器时会导致 HardFault 崩溃。
 */
#include <stdio.h>

/**
 * @brief fputc 重定向 - 覆盖 MicroLib 的 semihosting 默认实现
 *
 * 所有 printf 输出在此处被静默丢弃。
 * 项目中应使用 EasyLogger (log_i 等) 通过 UART 输出日志，
 * 而不是 printf。
 *
 * @note 仅影响 Keil 构建 (GCC 构建不编译此文件)
 */
int fputc(int ch, FILE *f) {
  (void)f;
  return ch; /* 丢弃输出，防止 semihosting HardFault */
}
