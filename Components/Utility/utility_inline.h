/**
 * @file utility_inline.h
 * @brief 内联工具函数 - 数据转换（小端/大端/BCD）
 * @version 1.0.0
 * @date 2025-12-31
 *
 * @note 这些函数都是 inline，直接在头文件中实现
 *       包含此文件即可使用，无需链接额外的 .c 文件
 */

#ifndef __UTILITY_INLINE_H__
#define __UTILITY_INLINE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 *                              小端格式读写
 *============================================================================*/

/**
 * @brief 读取小端格式的16位无符号整数
 * @param buf 数据缓冲区指针
 * @return 16位值
 */
static inline uint16_t util_read_le_u16(const uint8_t *buf) {
  return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

/**
 * @brief 读取小端格式的32位无符号整数
 * @param buf 数据缓冲区指针
 * @return 32位值
 */
static inline uint32_t util_read_le_u32(const uint8_t *buf) {
  return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
         ((uint32_t)buf[3] << 24);
}

/**
 * @brief 写入小端格式的16位无符号整数
 * @param buf 目标缓冲区指针
 * @param val 要写入的值
 */
static inline void util_write_le_u16(uint8_t *buf, uint16_t val) {
  buf[0] = val & 0xFF;
  buf[1] = (val >> 8) & 0xFF;
}

/**
 * @brief 写入小端格式的32位无符号整数
 * @param buf 目标缓冲区指针
 * @param val 要写入的值
 */
static inline void util_write_le_u32(uint8_t *buf, uint32_t val) {
  buf[0] = val & 0xFF;
  buf[1] = (val >> 8) & 0xFF;
  buf[2] = (val >> 16) & 0xFF;
  buf[3] = (val >> 24) & 0xFF;
}

/*============================================================================
 *                              大端格式读写
 *============================================================================*/

/**
 * @brief 读取大端格式的16位无符号整数
 * @param buf 数据缓冲区指针
 * @return 16位值
 */
static inline uint16_t util_read_be_u16(const uint8_t *buf) {
  return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

/**
 * @brief 读取大端格式的32位无符号整数
 * @param buf 数据缓冲区指针
 * @return 32位值
 */
static inline uint32_t util_read_be_u32(const uint8_t *buf) {
  return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
         ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

/**
 * @brief 写入大端格式的16位无符号整数
 * @param buf 目标缓冲区指针
 * @param val 要写入的值
 */
static inline void util_write_be_u16(uint8_t *buf, uint16_t val) {
  buf[0] = (val >> 8) & 0xFF;
  buf[1] = val & 0xFF;
}

/**
 * @brief 写入大端格式的32位无符号整数
 * @param buf 目标缓冲区指针
 * @param val 要写入的值
 */
static inline void util_write_be_u32(uint8_t *buf, uint32_t val) {
  buf[0] = (val >> 24) & 0xFF;
  buf[1] = (val >> 16) & 0xFF;
  buf[2] = (val >> 8) & 0xFF;
  buf[3] = val & 0xFF;
}

/*============================================================================
 *                              BCD 编码转换
 *============================================================================*/

/**
 * @brief BCD码转十进制
 * @param bcd BCD码值
 * @return 十进制值
 */
static inline uint8_t util_bcd_to_dec(uint8_t bcd) {
  return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/**
 * @brief 十进制转BCD码
 * @param dec 十进制值 (0-99)
 * @return BCD码值
 */
static inline uint8_t util_dec_to_bcd(uint8_t dec) {
  return ((dec / 10) << 4) | (dec % 10);
}

/**
 * @brief BCD数组转32位整数（小端）
 * @param bcd BCD数组指针
 * @param len 数组长度（1-4字节）
 * @return 转换后的整数值
 *
 * @example
 * uint8_t bcd[4] = {0x78, 0x56, 0x34, 0x12}; // 表示 12345678
 * uint32_t val = util_bcd_array_to_u32(bcd, 4); // = 12345678
 */
static inline uint32_t util_bcd_array_to_u32(const uint8_t *bcd, uint8_t len) {
  uint32_t result = 0;
  uint32_t multiplier = 1;

  for (uint8_t i = 0; i < len && i < 4; i++) {
    result += util_bcd_to_dec(bcd[i]) * multiplier;
    multiplier *= 100;
  }

  return result;
}

/*============================================================================
 *                              位操作
 *============================================================================*/

/**
 * @brief 设置指定位
 * @param val 原值
 * @param bit 位号 (0-31)
 * @return 设置后的值
 */
static inline uint32_t util_set_bit(uint32_t val, uint8_t bit) {
  return val | (1UL << bit);
}

/**
 * @brief 清除指定位
 * @param val 原值
 * @param bit 位号 (0-31)
 * @return 清除后的值
 */
static inline uint32_t util_clear_bit(uint32_t val, uint8_t bit) {
  return val & ~(1UL << bit);
}

/**
 * @brief 检查指定位是否设置
 * @param val 值
 * @param bit 位号 (0-31)
 * @return 1: 已设置, 0: 未设置
 */
static inline uint8_t util_test_bit(uint32_t val, uint8_t bit) {
  return (val & (1UL << bit)) ? 1 : 0;
}

/**
 * @brief 翻转指定位
 * @param val 原值
 * @param bit 位号 (0-31)
 * @return 翻转后的值
 */
static inline uint32_t util_toggle_bit(uint32_t val, uint8_t bit) {
  return val ^ (1UL << bit);
}

#ifdef __cplusplus
}
#endif

#endif /* __UTILITY_INLINE_H__ */
