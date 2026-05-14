/**
 * @file utility_convert.c
 * @brief 数据转换工具函数实现
 * @version 1.0.0
 * @date 2025-12-31
 */

#include "utility.h"

/*============================================================================
 *                              字节操作
 *============================================================================*/

void util_reverse_bytes(uint8_t *data, uint16_t len) {
  if (data == NULL || len <= 1) {
    return;
  }

  uint16_t i = 0;
  uint16_t j = len - 1;

  while (i < j) {
    uint8_t temp = data[i];
    data[i] = data[j];
    data[j] = temp;
    i++;
    j--;
  }
}

/*============================================================================
 *                              字符串转换
 *============================================================================*/

/**
 * @brief 十六进制字符转数值
 */
static uint8_t hex_char_to_val(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return 0;
}

uint16_t util_hex_str_to_bytes(const char *hex_str, uint8_t *out_buf,
                               uint16_t max_len) {
  if (hex_str == NULL || out_buf == NULL || max_len == 0) {
    return 0;
  }

  uint16_t out_idx = 0;
  uint16_t str_idx = 0;

  while (hex_str[str_idx] != '\0' && out_idx < max_len) {
    // 跳过空格和其他分隔符
    char c1 = hex_str[str_idx];
    if (c1 == ' ' || c1 == ':' || c1 == '-') {
      str_idx++;
      continue;
    }

    // 需要两个十六进制字符
    char c2 = hex_str[str_idx + 1];
    if (c2 == '\0') {
      break;
    }

    // 转换
    uint8_t high = hex_char_to_val(c1);
    uint8_t low = hex_char_to_val(c2);
    out_buf[out_idx++] = (high << 4) | low;

    str_idx += 2;
  }

  return out_idx;
}
