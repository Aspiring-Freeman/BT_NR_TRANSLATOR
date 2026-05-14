/**
 * @file retry_manager.c
 * @brief 统一重试管理模块 - 实现
 * @version 1.0.0
 * @date 2026-01-30
 */

#include "retry_manager.h"
#include "time_manager.h"
#include <string.h>

// #define LOG_TAG "retry_mgr"
// #include <elog.h>

/*============================================================================
 *                          内部状态
 *===========================================================================*/

static RM_State_t s_rm_state = {0};
static RM_ResetCallback s_reset_callback = NULL;
static RM_RetryActionCallback s_retry_action_callback = NULL;

/*============================================================================
 *                          API实现
 *===========================================================================*/

void RM_Init(uint8_t max_retry, uint32_t retry_delay_ms) {
  memset(&s_rm_state, 0, sizeof(s_rm_state));
  s_rm_state.max_retry = max_retry;
  s_rm_state.retry_delay_ms = retry_delay_ms;
  s_rm_state.retry_count = 0;
  s_rm_state.waiting_retry_delay = false;

  // log_d("重试管理器初始化: max_retry=%d, delay=%dms", max_retry,
  // retry_delay_ms);
}

void RM_SetResetCallback(RM_ResetCallback callback) {
  s_reset_callback = callback;
}

void RM_SetRetryActionCallback(RM_RetryActionCallback callback) {
  s_retry_action_callback = callback;
}

RM_RetryResult_t RM_TryRetry(RM_RetryReason_t reason) {
  (void)reason; // 避免未使用参数警告（日志禁用时）

  // 检查是否允许重试
  if (s_rm_state.max_retry == 0) {
    // log_d("该步骤不允许重试");
    return RM_RESULT_NO_RETRY_CONFIG;
  }

  // 检查重试次数
  if (s_rm_state.retry_count >= s_rm_state.max_retry) {
    // log_w("重试次数已用尽 (%d/%d)", s_rm_state.retry_count,
    // s_rm_state.max_retry);
    return RM_RESULT_RETRY_EXHAUSTED;
  }

  // 增加重试计数
  s_rm_state.retry_count++;
  // log_i("触发重试 %d/%d, 原因: %s",
  //       s_rm_state.retry_count,
  //       s_rm_state.max_retry,
  //       RM_GetReasonString(reason));

  // 调用状态重置回调
  if (s_reset_callback != NULL) {
    s_reset_callback();
  }

  // 重置单步超时
  TM_ResetStepTimeout();

  // 检查是否需要延时
  if (s_rm_state.retry_delay_ms > 0) {
    TM_SetDelay(s_rm_state.retry_delay_ms);
    s_rm_state.waiting_retry_delay = true;
    // log_d("等待重试延时 %dms", s_rm_state.retry_delay_ms);
  } else {
    // 立即执行重试动作
    if (s_retry_action_callback != NULL) {
      s_retry_action_callback();
    }
  }

  return RM_RESULT_RETRY_OK;
}

bool RM_IsWaitingRetryDelay(void) { return s_rm_state.waiting_retry_delay; }

bool RM_CheckRetryDelayComplete(void) {
  if (!s_rm_state.waiting_retry_delay) {
    return false;
  }

  if (TM_IsDelayComplete()) {
    s_rm_state.waiting_retry_delay = false;

    // 延时完成，执行重试动作
    if (s_retry_action_callback != NULL) {
      // log_d("重试延时完成，执行重试动作");
      s_retry_action_callback();
    }
    return true;
  }

  return false;
}

uint8_t RM_GetRetryCount(void) { return s_rm_state.retry_count; }

uint8_t RM_GetRetryRemaining(void) {
  if (s_rm_state.retry_count >= s_rm_state.max_retry) {
    return 0;
  }
  return s_rm_state.max_retry - s_rm_state.retry_count;
}

void RM_Reset(void) {
  s_rm_state.retry_count = 0;
  s_rm_state.waiting_retry_delay = false;
}

void RM_CancelRetryDelay(void) {
  if (s_rm_state.waiting_retry_delay) {
    s_rm_state.waiting_retry_delay = false;
    TM_CancelDelay();
    // log_d("重试延时已取消");
  }
}

const char *RM_GetReasonString(RM_RetryReason_t reason) {
  switch (reason) {
  case RM_REASON_TIMEOUT:
    return "超时";
  case RM_REASON_RESPONSE_INVALID:
    return "响应无效";
  case RM_REASON_CHECK_FAILED:
    return "检测失败";
  case RM_REASON_NO_RESPONSE:
    return "无响应";
  case RM_REASON_COMM_ERROR:
    return "通信错误";
  default:
    return "未知";
  }
}
