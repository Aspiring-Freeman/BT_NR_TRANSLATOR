/**
 * @file retry_manager.h
 * @brief 统一重试管理模块
 * @version 1.0.0
 * @date 2026-01-30
 *
 * @section problem 解决的问题
 * 1. 超时重试和失败重试逻辑分散 → 统一入口
 * 2. 重试时状态未完全重置 → 自动重置所有相关状态
 * 3. 重试计数混乱 → 单一计数器，明确语义
 *
 * @section design 设计原则
 * - 重试只有一个触发入口：RM_TryRetry()
 * - 无论是超时还是失败，都调用同一个函数
 * - 重试时自动重置：超时、事件、执行状态、子状态
 */

#ifndef __RETRY_MANAGER_H__
#define __RETRY_MANAGER_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 *                          重试原因枚举
 *===========================================================================*/

/**
 * @brief 重试原因（用于日志和调试）
 */
typedef enum {
  RM_REASON_TIMEOUT,          /**< 超时触发重试 */
  RM_REASON_RESPONSE_INVALID, /**< 响应数据无效 */
  RM_REASON_CHECK_FAILED,     /**< 检测结果失败（如电压超范围）*/
  RM_REASON_NO_RESPONSE,      /**< 无响应 */
  RM_REASON_COMM_ERROR,       /**< 通信错误 */
} RM_RetryReason_t;

/*============================================================================
 *                          重试结果枚举
 *===========================================================================*/

/**
 * @brief 重试尝试结果
 */
typedef enum {
  RM_RESULT_RETRY_OK,        /**< 可以重试，已重置状态 */
  RM_RESULT_RETRY_EXHAUSTED, /**< 重试次数已用尽 */
  RM_RESULT_NO_RETRY_CONFIG, /**< 该步骤不允许重试 (max_retry=0) */
} RM_RetryResult_t;

/*============================================================================
 *                          重试配置结构体
 *===========================================================================*/

/**
 * @brief 重试管理器状态
 */
typedef struct {
  uint8_t retry_count;      /**< 当前重试次数 */
  uint8_t max_retry;        /**< 最大重试次数 */
  uint32_t retry_delay_ms;  /**< 重试前延时(ms)，0表示立即重试 */
  bool waiting_retry_delay; /**< 是否正在等待重试延时 */
} RM_State_t;

/*============================================================================
 *                          回调函数类型
 *===========================================================================*/

/**
 * @brief 状态重置回调
 * @note 重试时调用，用于重置测试模块的内部状态
 */
typedef void (*RM_ResetCallback)(void);

/**
 * @brief 重试动作回调
 * @note 重置完成后调用，通常是重新发送请求
 */
typedef void (*RM_RetryActionCallback)(void);

/*============================================================================
 *                          API
 *===========================================================================*/

/**
 * @brief 初始化重试管理器（进入新步骤时调用）
 * @param max_retry 最大重试次数，0表示不重试
 * @param retry_delay_ms 重试前延时，0表示立即重试
 */
void RM_Init(uint8_t max_retry, uint32_t retry_delay_ms);

/**
 * @brief 设置状态重置回调
 * @param callback 回调函数
 */
void RM_SetResetCallback(RM_ResetCallback callback);

/**
 * @brief 设置重试动作回调
 * @param callback 回调函数
 */
void RM_SetRetryActionCallback(RM_RetryActionCallback callback);

/**
 * @brief 尝试重试（统一入口）
 * @param reason 重试原因
 * @return RM_RetryResult_t 重试结果
 *
 * @note 这是重试的唯一入口！
 *       无论是超时、失败、通信错误，都调用此函数
 *
 * @code
 * // 示例1：超时时
 * if (TM_IsStepTimeout()) {
 *     RM_RetryResult_t result = RM_TryRetry(RM_REASON_TIMEOUT);
 *     if (result == RM_RESULT_RETRY_EXHAUSTED) {
 *         fail_test();
 *     }
 *     return;  // 重要：return让主循环继续
 * }
 *
 * // 示例2：数据校验失败时
 * if (!validate_response(data)) {
 *     RM_RetryResult_t result = RM_TryRetry(RM_REASON_RESPONSE_INVALID);
 *     if (result == RM_RESULT_RETRY_EXHAUSTED) {
 *         fail_test();
 *     }
 *     return;
 * }
 * @endcode
 */
RM_RetryResult_t RM_TryRetry(RM_RetryReason_t reason);

/**
 * @brief 检查是否正在等待重试延时
 * @return true 正在等待延时
 */
bool RM_IsWaitingRetryDelay(void);

/**
 * @brief 检查重试延时是否完成
 * @return true 延时完成，应执行重试动作
 * @note 返回true后自动执行重试动作回调
 */
bool RM_CheckRetryDelayComplete(void);

/**
 * @brief 获取当前重试次数
 * @return 重试次数
 */
uint8_t RM_GetRetryCount(void);

/**
 * @brief 获取剩余重试次数
 * @return 剩余次数
 */
uint8_t RM_GetRetryRemaining(void);

/**
 * @brief 重置重试计数器（步骤成功时调用）
 */
void RM_Reset(void);

/**
 * @brief 取消重试延时（收到响应时调用）
 * @note 当等待重试延时期间收到有效响应时调用，立即取消等待
 */
void RM_CancelRetryDelay(void);

/**
 * @brief 获取重试原因字符串（调试用）
 * @param reason 重试原因
 * @return 字符串描述
 */
const char *RM_GetReasonString(RM_RetryReason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* __RETRY_MANAGER_H__ */
