/**
 * @file utility_filter.c
 * @brief 滤波和去极值算法实现
 * @version 1.0.0
 * @date 2025-12-31
 *
 * @section intro 简介
 * 提供多种滤波算法，用于功耗检测等场景的数据处理。
 *
 * @section algorithms 算法说明
 *
 * 1. 去极值滤波 (util_filter_remove_extreme)
 *    - 去掉最高和最低的若干个采样值
 *    - 用剩余的值计算平均
 *    - 适合有偶发异常值的场景
 *
 * 2. 中位数滤波 (util_filter_median)
 *    - 返回排序后的中间值
 *    - 完全不受极端值影响
 *    - 适合噪声较大的场景
 *
 * 3. 限幅滤波 (util_filter_clamp)
 *    - 超出范围的值用边界值替代
 *    - 然后计算平均值
 *    - 适合有明确有效范围的场景
 */

#include "utility.h"

/*============================================================================
 *                              内部辅助函数
 *============================================================================*/

/**
 * @brief 快速排序分区函数
 */
static int16_t partition(uint16_t *arr, int16_t low, int16_t high) {
  uint16_t pivot = arr[high];
  int16_t i = low - 1;

  for (int16_t j = low; j < high; j++) {
    if (arr[j] <= pivot) {
      i++;
      // 交换
      uint16_t temp = arr[i];
      arr[i] = arr[j];
      arr[j] = temp;
    }
  }

  // 交换
  uint16_t temp = arr[i + 1];
  arr[i + 1] = arr[high];
  arr[high] = temp;

  return i + 1;
}

/**
 * @brief 快速排序实现
 */
static void quicksort(uint16_t *arr, int16_t low, int16_t high) {
  if (low < high) {
    int16_t pi = partition(arr, low, high);
    quicksort(arr, low, pi - 1);
    quicksort(arr, pi + 1, high);
  }
}

/**
 * @brief 对数组进行排序（原地排序，会修改原数组）
 */
static void sort_array(uint16_t *arr, uint8_t count) {
  if (arr == NULL || count <= 1) {
    return;
  }
  quicksort(arr, 0, count - 1);
}

/*============================================================================
 *                              滤波算法实现
 *============================================================================*/

uint16_t util_filter_remove_extreme(uint16_t *samples, uint8_t count,
                                    uint8_t remove_high, uint8_t remove_low) {
  // 参数检查
  if (samples == NULL || count == 0) {
    return 0;
  }

  // 确保去掉的数量不超过总数
  uint8_t total_remove = remove_high + remove_low;
  if (total_remove >= count) {
    // 如果要去掉的数量大于等于总数，返回简单平均
    return util_filter_average(samples, count);
  }

  // 创建临时数组用于排序（避免修改原数组）
  uint16_t temp[32]; // 最多支持32个采样
  if (count > 32) {
    count = 32;
  }

  for (uint8_t i = 0; i < count; i++) {
    temp[i] = samples[i];
  }

  // 排序
  sort_array(temp, count);

  // 计算去极值后的平均值
  uint32_t sum = 0;
  uint8_t valid_count = count - total_remove;

  for (uint8_t i = remove_low; i < count - remove_high; i++) {
    sum += temp[i];
  }

  return (uint16_t)(sum / valid_count);
}

uint16_t util_filter_median(uint16_t *samples, uint8_t count) {
  // 参数检查
  if (samples == NULL || count == 0) {
    return 0;
  }

  if (count == 1) {
    return samples[0];
  }

  // 创建临时数组用于排序
  uint16_t temp[32];
  if (count > 32) {
    count = 32;
  }

  for (uint8_t i = 0; i < count; i++) {
    temp[i] = samples[i];
  }

  // 排序
  sort_array(temp, count);

  // 返回中位数
  if (count % 2 == 1) {
    // 奇数个，返回中间值
    return temp[count / 2];
  } else {
    // 偶数个，返回中间两个的平均值
    return (temp[count / 2 - 1] + temp[count / 2]) / 2;
  }
}

uint16_t util_filter_average(const uint16_t *samples, uint8_t count) {
  if (samples == NULL || count == 0) {
    return 0;
  }

  uint32_t sum = 0;
  for (uint8_t i = 0; i < count; i++) {
    sum += samples[i];
  }

  return (uint16_t)(sum / count);
}

uint16_t util_filter_clamp(uint16_t *samples, uint8_t count, uint16_t min_val,
                           uint16_t max_val) {
  if (samples == NULL || count == 0) {
    return 0;
  }

  uint32_t sum = 0;

  for (uint8_t i = 0; i < count; i++) {
    uint16_t val = samples[i];

    // 限幅
    if (val < min_val) {
      val = min_val;
    } else if (val > max_val) {
      val = max_val;
    }

    sum += val;
  }

  return (uint16_t)(sum / count);
}
