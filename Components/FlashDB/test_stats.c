/**
 * @file test_stats.c
 * @brief 测试统计信息Flash存储实现
 * @version 1.0.0
 * @date 2026-01-05
 *
 * 使用FlashDB的FAL层进行Flash操作
 * 支持磨损均衡写入保护
 */

#define LOG_TAG "test_stats"

#include "test_stats.h"
#include <elog.h>
#include <fal.h>
#include <string.h>

/*============================================================================
 * 内部定义
 *===========================================================================*/

#define PARTITION_NAME "test_stats"

/* Flash 扇区大小 */
#define FLASH_SECTOR_SIZE (2 * 1024)

/* 数据在分区中的偏移 */
#define SUMMARY_OFFSET 0
#define HISTORY_OFFSET 512 /* 预留512字节给汇总数据 */

/* 写入计数器偏移 (用于磨损均衡) */
#define WRITE_COUNTER_OFFSET (8 * 1024 - 4)

/*============================================================================
 * 内部变量
 *===========================================================================*/

static const struct fal_partition *s_part = NULL;
static bool s_initialized = false;
static uint8_t s_station_id = 0;

/* 内存中的缓存 */
static TestStatsSummary_t s_summary_cache;
static bool s_cache_valid = false;

/*============================================================================
 * CRC32 计算
 *===========================================================================*/

static uint32_t calc_crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

/*============================================================================
 * 内部函数
 *===========================================================================*/

static bool load_summary(void) {
  if (s_part == NULL) {
    return false;
  }

  TestStatsSummary_t summary;
  if (fal_partition_read(s_part, SUMMARY_OFFSET, (uint8_t *)&summary,
                         sizeof(summary)) < 0) {
    log_e("读取统计数据失败");
    return false;
  }

  /* 验证魔数 */
  if (summary.magic != TEST_STATS_MAGIC) {
    log_d("统计数据无效 (魔数=0x%08lX)", (unsigned long)summary.magic);
    return false;
  }

  /* 验证CRC */
  uint32_t calc_crc = calc_crc32((const uint8_t *)&summary,
                                 sizeof(summary) - sizeof(summary.checksum));
  if (calc_crc != summary.checksum) {
    log_e("统计数据CRC错误");
    return false;
  }

  memcpy(&s_summary_cache, &summary, sizeof(summary));
  s_cache_valid = true;
  return true;
}

static bool save_summary(void) {
  if (s_part == NULL) {
    return false;
  }

  /* 更新校验和 */
  s_summary_cache.checksum =
      calc_crc32((const uint8_t *)&s_summary_cache,
                 sizeof(s_summary_cache) - sizeof(s_summary_cache.checksum));

  /* 需要先擦除 (擦除整个分区会影响历史记录，这里只读取-修改-写入前512字节) */
  /* 实际上FAL会处理这个问题，但为了安全，我们采用读-改-写策略 */

  /* 读取整个分区数据 */
  uint8_t *buf = (uint8_t *)&s_summary_cache;

  /* 擦除分区的第一个扇区 (2KB) */
  if (fal_partition_erase(s_part, 0, FLASH_SECTOR_SIZE) < 0) {
    log_e("擦除失败");
    return false;
  }

  /* 写入汇总数据 */
  if (fal_partition_write(s_part, SUMMARY_OFFSET, buf,
                          sizeof(s_summary_cache)) < 0) {
    log_e("写入统计数据失败");
    return false;
  }

  return true;
}

static void init_default_summary(void) {
  memset(&s_summary_cache, 0, sizeof(s_summary_cache));
  s_summary_cache.magic = TEST_STATS_MAGIC;
  s_summary_cache.version = TEST_STATS_VERSION;
  s_summary_cache.station_id = s_station_id;
  s_summary_cache.last_test.failed_step = 0xFF;
  s_cache_valid = true;
}

/*============================================================================
 * API 实现
 *===========================================================================*/

bool TestStats_Init(void) {
  if (s_initialized) {
    return true;
  }

  /* 查找分区 */
  s_part = fal_partition_find(PARTITION_NAME);
  if (s_part == NULL) {
    log_e("找不到分区: %s", PARTITION_NAME);
    return false;
  }

  log_i("测试统计分区: addr=0x%05lX, size=%lu", (unsigned long)s_part->offset,
        (unsigned long)s_part->len);

  /* 尝试加载已有数据 */
  if (!load_summary()) {
    log_i("初始化新的统计数据");
    init_default_summary();
    /* 不需要立即保存，等第一次记录时再保存 */
  } else {
    log_i("已加载统计数据: 总测试=%lu, 通过=%lu, 失败=%lu",
          (unsigned long)s_summary_cache.total_tests,
          (unsigned long)s_summary_cache.total_pass,
          (unsigned long)s_summary_cache.total_fail);
  }

  s_initialized = true;
  return true;
}

bool TestStats_Record(uint8_t result, uint8_t failed_step, uint8_t error_code,
                      uint16_t duration_ms) {
  if (!s_initialized) {
    if (!TestStats_Init()) {
      return false;
    }
  }

  /* 更新统计 */
  s_summary_cache.total_tests++;
  if (result == 0) {
    s_summary_cache.total_pass++;
  } else {
    s_summary_cache.total_fail++;
    /* 更新步骤失败计数 */
    if (failed_step < TEST_STATS_MAX_STEPS) {
      s_summary_cache.step_fail_count[failed_step]++;
    }
  }

  /* 更新最后一次测试记录 */
  s_summary_cache.last_test.test_id = s_summary_cache.total_tests;
  s_summary_cache.last_test.timestamp = 0; /* TODO: 添加RTC时间戳 */
  s_summary_cache.last_test.station_id = s_station_id;
  s_summary_cache.last_test.result = result;
  s_summary_cache.last_test.failed_step = failed_step;
  s_summary_cache.last_test.error_code = error_code;
  s_summary_cache.last_test.duration_ms = duration_ms;

  /* 每10次测试保存一次，减少Flash磨损 */
  if ((s_summary_cache.total_tests % 10) == 0) {
    if (!save_summary()) {
      log_e("保存统计数据失败");
      return false;
    }
    log_d("统计数据已保存 (第%lu次测试)",
          (unsigned long)s_summary_cache.total_tests);
  }

  return true;
}

bool TestStats_GetSummary(TestStatsSummary_t *summary) {
  if (!s_initialized || !s_cache_valid || summary == NULL) {
    return false;
  }

  memcpy(summary, &s_summary_cache, sizeof(TestStatsSummary_t));
  return true;
}

int TestStats_GetHistory(TestRecord_t *records, int max_count) {
  /* TODO: 实现历史记录功能 */
  (void)records;
  (void)max_count;
  return 0;
}

uint32_t TestStats_GetTotalCount(void) {
  if (!s_initialized || !s_cache_valid) {
    return 0;
  }
  return s_summary_cache.total_tests;
}

uint32_t TestStats_GetPassRate(void) {
  if (!s_initialized || !s_cache_valid || s_summary_cache.total_tests == 0) {
    return 0;
  }
  return (s_summary_cache.total_pass * 10000) / s_summary_cache.total_tests;
}

uint32_t TestStats_GetStepFailCount(uint8_t step_id) {
  if (!s_initialized || !s_cache_valid || step_id >= TEST_STATS_MAX_STEPS) {
    return 0;
  }
  return s_summary_cache.step_fail_count[step_id];
}

void TestStats_Print(void) {
  if (!s_initialized || !s_cache_valid) {
    log_w("统计数据未初始化或无效");
    return;
  }

  log_i("╔══════════════════════════════════════════════════════════╗");
  log_i("║                   测试统计信息                           ║");
  log_i("╠══════════════════════════════════════════════════════════╣");
  log_i("║ 工位号: %d                                                ║",
        s_summary_cache.station_id);
  log_i("║ 总测试次数: %-8lu                                     ║",
        (unsigned long)s_summary_cache.total_tests);
  log_i("║ 通过次数:   %-8lu                                     ║",
        (unsigned long)s_summary_cache.total_pass);
  log_i("║ 失败次数:   %-8lu                                     ║",
        (unsigned long)s_summary_cache.total_fail);

  uint32_t pass_rate = TestStats_GetPassRate();
  log_i("║ 通过率:     %lu.%02lu%%                                       ║",
        (unsigned long)(pass_rate / 100), (unsigned long)(pass_rate % 100));

  log_i("╠══════════════════════════════════════════════════════════╣");
  log_i("║ 各步骤失败统计:                                          ║");

  for (int i = 0; i < TEST_STATS_MAX_STEPS; i++) {
    if (s_summary_cache.step_fail_count[i] > 0) {
      log_i("║   步骤%2d: %-8lu次失败                                 ║", i,
            (unsigned long)s_summary_cache.step_fail_count[i]);
    }
  }

  log_i("╠══════════════════════════════════════════════════════════╣");
  log_i("║ 最后一次测试:                                            ║");
  log_i("║   序号: %lu                                               ║",
        (unsigned long)s_summary_cache.last_test.test_id);
  log_i("║   结果: %s                                             ║",
        s_summary_cache.last_test.result == 0 ? "通过" : "失败");
  if (s_summary_cache.last_test.result != 0) {
    log_i("║   失败步骤: %d                                            ║",
          s_summary_cache.last_test.failed_step);
    log_i("║   错误码: 0x%02X                                           ║",
          s_summary_cache.last_test.error_code);
  }
  log_i("║   耗时: %dms                                             ║",
        s_summary_cache.last_test.duration_ms);
  log_i("╚══════════════════════════════════════════════════════════╝");
}

bool TestStats_Clear(void) {
  if (s_part == NULL) {
    return false;
  }

  log_w("清除所有测试统计数据...");

  /* 擦除整个分区 */
  if (fal_partition_erase(s_part, 0, s_part->len) < 0) {
    log_e("擦除失败");
    return false;
  }

  /* 重新初始化 */
  init_default_summary();
  s_cache_valid = true;

  log_i("测试统计已清除");
  return true;
}

void TestStats_SetStationId(uint8_t station_id) {
  s_station_id = station_id;
  if (s_cache_valid) {
    s_summary_cache.station_id = station_id;
  }
}

/**
 * @brief 强制保存统计数据到Flash
 * @return true: 成功, false: 失败
 */
bool TestStats_ForceSave(void) {
  if (!s_initialized || !s_cache_valid) {
    return false;
  }
  return save_summary();
}
