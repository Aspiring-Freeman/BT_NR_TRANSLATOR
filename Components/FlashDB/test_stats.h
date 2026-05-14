/**
 * @file test_stats.h
 * @brief 测试统计信息Flash存储接口
 * @version 1.0.0
 * @date 2026-01-05
 *
 * 提供测试统计数据的持久化存储功能：
 * - 总测试次数
 * - 各步骤失败次数统计
 * - 最近N次测试记录
 * - 磨损均衡写入保护
 */

#ifndef __TEST_STATS_H__
#define __TEST_STATS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*============================================================================
 * 配置定义
 *===========================================================================*/

/** 最大测试步骤数 */
#define TEST_STATS_MAX_STEPS 16

/** 保存的历史记录条数 */
#define TEST_STATS_HISTORY_COUNT 32

/** 统计数据版本 */
#define TEST_STATS_VERSION 0x01

/** 魔数 */
#define TEST_STATS_MAGIC 0x54455354 /* "TEST" */

/*============================================================================
 * 数据结构定义
 *===========================================================================*/

/**
 * @brief 单次测试记录
 */
#pragma pack(1)
typedef struct {
  uint32_t test_id;     /**< 测试序号 */
  uint32_t timestamp;   /**< 时间戳 (系统tick或RTC) */
  uint8_t station_id;   /**< 工位号 */
  uint8_t result;       /**< 测试结果: 0=通过, 1=失败 */
  uint8_t failed_step;  /**< 失败的步骤号 (0xFF=无) */
  uint8_t error_code;   /**< 错误码 */
  uint16_t duration_ms; /**< 测试耗时(ms) */
  uint16_t reserved;    /**< 保留 */
} TestRecord_t;
#pragma pack()

/**
 * @brief 测试统计汇总数据
 */
#pragma pack(1)
typedef struct {
  uint32_t magic;     /**< 魔数: 0x54455354 "TEST" */
  uint8_t version;    /**< 数据版本 */
  uint8_t station_id; /**< 工位号 */
  uint16_t reserved;  /**< 保留 */

  /* 总计统计 */
  uint32_t total_tests; /**< 总测试次数 */
  uint32_t total_pass;  /**< 总通过次数 */
  uint32_t total_fail;  /**< 总失败次数 */

  /* 各步骤失败统计 */
  uint32_t step_fail_count[TEST_STATS_MAX_STEPS]; /**< 各步骤失败次数 */

  /* 最近一次测试信息 */
  TestRecord_t last_test; /**< 最后一次测试记录 */

  /* 校验 */
  uint32_t checksum; /**< CRC32校验 */
} TestStatsSummary_t;
#pragma pack()

/**
 * @brief 测试历史记录 (环形缓冲)
 */
#pragma pack(1)
typedef struct {
  uint32_t magic;                                 /**< 魔数 */
  uint8_t version;                                /**< 版本 */
  uint8_t head_index;                             /**< 写入位置 */
  uint8_t count;                                  /**< 有效记录数 */
  uint8_t reserved;                               /**< 保留 */
  TestRecord_t records[TEST_STATS_HISTORY_COUNT]; /**< 历史记录 */
  uint32_t checksum;                              /**< CRC32校验 */
} TestStatsHistory_t;
#pragma pack()

/*============================================================================
 * API 函数
 *===========================================================================*/

/**
 * @brief 初始化测试统计模块
 * @return true: 成功, false: 失败
 */
bool TestStats_Init(void);

/**
 * @brief 记录一次测试结果
 *
 * @param result 测试结果: 0=通过, 非0=失败
 * @param failed_step 失败的步骤号 (通过时填0xFF)
 * @param error_code 错误码
 * @param duration_ms 测试耗时(ms)
 * @return true: 成功, false: 失败
 */
bool TestStats_Record(uint8_t result, uint8_t failed_step, uint8_t error_code,
                      uint16_t duration_ms);

/**
 * @brief 获取统计汇总信息
 * @param summary 输出统计信息
 * @return true: 成功, false: 失败或无数据
 */
bool TestStats_GetSummary(TestStatsSummary_t *summary);

/**
 * @brief 获取最近N条测试记录
 * @param records 输出记录数组
 * @param max_count 最大记录数
 * @return 实际返回的记录数
 */
int TestStats_GetHistory(TestRecord_t *records, int max_count);

/**
 * @brief 获取总测试次数
 * @return 总测试次数，失败返回0
 */
uint32_t TestStats_GetTotalCount(void);

/**
 * @brief 获取通过率 (百分比 * 100)
 * @return 通过率，如 9523 表示 95.23%
 */
uint32_t TestStats_GetPassRate(void);

/**
 * @brief 获取指定步骤的失败次数
 * @param step_id 步骤号 (0-15)
 * @return 失败次数
 */
uint32_t TestStats_GetStepFailCount(uint8_t step_id);

/**
 * @brief 打印统计信息到日志
 */
void TestStats_Print(void);

/**
 * @brief 清除所有统计数据
 * @return true: 成功, false: 失败
 */
bool TestStats_Clear(void);

/**
 * @brief 设置工位号
 * @param station_id 工位号
 */
void TestStats_SetStationId(uint8_t station_id);

#ifdef __cplusplus
}
#endif

#endif /* __TEST_STATS_H__ */
