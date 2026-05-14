/**
 * @file utility.h
 * @brief 通用工具库 - 统一头文件
 * @version 1.0.0
 * @date 2025-12-31
 *
 * @section intro 简介
 * 提供项目通用的工具函数，包括：
 * - CRC/校验和计算
 * - 滤波/去极值算法
 * - 数据格式转换
 *
 * @section usage 使用方法
 * @code
 * #include "Utility/utility.h"
 *
 * // CRC计算
 * uint16_t crc = util_crc16_modbus(data, len);
 * uint8_t sum = util_checksum_sum8(data, len);
 *
 * // 去极值滤波
 * uint16_t avg = util_filter_remove_extreme(samples, 10, 1, 1);
 *
 * // 数据转换
 * uint16_t val = util_read_le_u16(buf);
 * @endcode
 */

#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 *                              CRC/校验和计算
 *============================================================================*/

/**
 * @brief 计算CRC16-Modbus校验值
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return CRC16值（小端格式）
 */
uint16_t util_crc16_modbus(const uint8_t *data, uint16_t len);

/**
 * @brief 计算CRC16-CCITT校验值
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return CRC16值
 * @note 多项式: x^16 + x^12 + x^5 + 1, 初始值: 0x0000
 */
uint16_t util_crc16_ccitt(const uint8_t *data, uint16_t len);

/**
 * @brief 计算8位累加和校验
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 累加和（低8位）
 */
uint8_t util_checksum_sum8(const uint8_t *data, uint16_t len);

/**
 * @brief 计算16位累加和校验
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 累加和（低16位）
 */
uint16_t util_checksum_sum16(const uint8_t *data, uint16_t len);

/*============================================================================
 *                              滤波/去极值算法
 *============================================================================*/

/**
 * @brief 去极值滤波 - 去掉最高和最低的若干个值后计算平均
 * @param samples 采样数组
 * @param count 采样数量
 * @param remove_high 去掉最高的几个值
 * @param remove_low 去掉最低的几个值
 * @return 滤波后的平均值
 *
 * @example
 * uint16_t data[10] = {24, 21, 54, 37, 29, 25, 22, 45, 41, 32};
 * uint16_t avg = util_filter_remove_extreme(data, 10, 1, 1);
 * // 去掉最高1个(54)和最低1个(21)，剩余8个平均 = 31
 */
uint16_t util_filter_remove_extreme(uint16_t *samples, uint8_t count,
                                    uint8_t remove_high, uint8_t remove_low);

/**
 * @brief 中位数滤波 - 返回采样数组的中位数
 * @param samples 采样数组
 * @param count 采样数量
 * @return 中位数值
 *
 * @note 如果count为偶数，返回中间两个数的平均值
 */
uint16_t util_filter_median(uint16_t *samples, uint8_t count);

/**
 * @brief 简单平均值计算
 * @param samples 采样数组
 * @param count 采样数量
 * @return 平均值
 */
uint16_t util_filter_average(const uint16_t *samples, uint8_t count);

/**
 * @brief 限幅滤波 - 超出范围的值用边界值替代
 * @param samples 采样数组
 * @param count 采样数量
 * @param min_val 最小允许值
 * @param max_val 最大允许值
 * @return 滤波后的平均值
 */
uint16_t util_filter_clamp(uint16_t *samples, uint8_t count, uint16_t min_val,
                           uint16_t max_val);

/*============================================================================
 *                              数据格式转换（非内联函数）
 *============================================================================*/

/**
 * @brief 字节数组反转
 * @param data 数据缓冲区
 * @param len 数据长度
 */
void util_reverse_bytes(uint8_t *data, uint16_t len);

/**
 * @brief 十六进制字符串转字节数组
 * @param hex_str 十六进制字符串 (如 "68AA19...")
 * @param out_buf 输出缓冲区
 * @param max_len 输出缓冲区最大长度
 * @return 实际转换的字节数
 */
uint16_t util_hex_str_to_bytes(const char *hex_str, uint8_t *out_buf,
                               uint16_t max_len);

#ifdef __cplusplus
}
#endif

/*============================================================================
 *                   内联函数（小端/大端/BCD/位操作）
 *============================================================================*/
#include "utility_inline.h"

#endif /* __UTILITY_H__ */
