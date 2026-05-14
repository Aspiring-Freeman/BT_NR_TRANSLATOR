/**
 * @file time_manager.c
 * @brief 统一时间管理模块 - 实现
 * @version 2.0.0
 * @date 2026-01-30
 */

#include "time_manager.h"
#include <string.h>

/* 如果需要日志，取消注释 */
// #define LOG_TAG "time_mgr"
// #include <elog.h>

/*============================================================================
 *                          内部状态
 *===========================================================================*/

/** @brief 时间管理器内部状态 */
static TM_State_t s_tm_state = {0};

/** @brief 初始化标志 */
static bool s_initialized = false;

/*============================================================================
 *                          基础API实现
 *===========================================================================*/

void TM_Init(void) {
  /* 清零所有状态 */
  memset(&s_tm_state, 0, sizeof(s_tm_state));

  /* 初始化周期任务默认间隔 */
  s_tm_state.period_interval[TM_PERIOD_ID_LED] = TM_PERIOD_LED_BLINK;
  s_tm_state.period_interval[TM_PERIOD_ID_POWER] = TM_PERIOD_POWER_CHECK;
  s_tm_state.period_interval[TM_PERIOD_ID_DEBUG] = TM_PERIOD_DEBUG_PRINT;
  s_tm_state.period_interval[TM_PERIOD_ID_WATCHDOG] = TM_PERIOD_WATCHDOG;

  s_initialized = true;

  // log_i("时间管理器初始化完成 v%d.%d.%d",
  //       TIME_MANAGER_VERSION_MAJOR,
  //       TIME_MANAGER_VERSION_MINOR,
  //       TIME_MANAGER_VERSION_PATCH);
}

void TM_SysTick_Handler(void) {
  if (!s_initialized) {
    return;
  }

  /* 系统滴答递增 */
  s_tm_state.sys_tick++;

  /* 注意：超时检测在 TM_IsXxxTimeout() 中进行，而不是在中断中 */
  /* 这样可以避免中断中做复杂计算 */
}

uint32_t TM_GetTick(void) { return s_tm_state.sys_tick; }

uint32_t TM_GetElapsed(uint32_t start_tick) {
  uint32_t current = s_tm_state.sys_tick;

  /* 处理溢出情况 */
  if (current >= start_tick) {
    return current - start_tick;
  } else {
    /* 溢出：0xFFFFFFFF - start + current + 1 */
    return (0xFFFFFFFF - start_tick) + current + 1;
  }
}

/*============================================================================
 *                          全局超时API实现
 *===========================================================================*/

void TM_StartGlobalTimeout(uint32_t timeout_ms) {
  s_tm_state.global_timeout_ms =
      (timeout_ms > 0) ? timeout_ms : TM_TIMEOUT_GLOBAL_TEST;
  s_tm_state.global_start_tick = s_tm_state.sys_tick;
  s_tm_state.global_timeout_active = true;

  // log_d("全局超时启动: %lu ms", (unsigned long)s_tm_state.global_timeout_ms);
}

void TM_StopGlobalTimeout(void) {
  s_tm_state.global_timeout_active = false;
  // log_d("全局超时停止");
}

bool TM_IsGlobalTimeout(void) {
  if (!s_tm_state.global_timeout_active) {
    return false;
  }

  uint32_t elapsed = TM_GetElapsed(s_tm_state.global_start_tick);
  return (elapsed >= s_tm_state.global_timeout_ms);
}

uint32_t TM_GetGlobalRemaining(void) {
  if (!s_tm_state.global_timeout_active) {
    return 0;
  }

  uint32_t elapsed = TM_GetElapsed(s_tm_state.global_start_tick);
  if (elapsed >= s_tm_state.global_timeout_ms) {
    return 0;
  }
  return s_tm_state.global_timeout_ms - elapsed;
}

/*============================================================================
 *                          单步超时API实现
 *===========================================================================*/

void TM_SetStepTimeout(uint32_t timeout_ms) {
  s_tm_state.step_timeout_ms = timeout_ms;
  s_tm_state.step_start_tick = s_tm_state.sys_tick;
  s_tm_state.step_timeout_active = true;

  // log_d("单步超时设置: %lu ms", (unsigned long)timeout_ms);
}

void TM_ResetStepTimeout(void) {
  if (s_tm_state.step_timeout_active) {
    s_tm_state.step_start_tick = s_tm_state.sys_tick;
    // log_d("单步超时重置");
  }
}

void TM_StopStepTimeout(void) { s_tm_state.step_timeout_active = false; }

bool TM_IsStepTimeout(void) {
  if (!s_tm_state.step_timeout_active) {
    return false;
  }

  uint32_t elapsed = TM_GetElapsed(s_tm_state.step_start_tick);
  return (elapsed >= s_tm_state.step_timeout_ms);
}

uint32_t TM_GetStepRemaining(void) {
  if (!s_tm_state.step_timeout_active) {
    return 0;
  }

  uint32_t elapsed = TM_GetElapsed(s_tm_state.step_start_tick);
  if (elapsed >= s_tm_state.step_timeout_ms) {
    return 0;
  }
  return s_tm_state.step_timeout_ms - elapsed;
}

TM_TimeoutResult_t TM_CheckTimeout(void) {
  /* 全局超时优先级最高 */
  if (TM_IsGlobalTimeout()) {
    return TM_TIMEOUT_GLOBAL;
  }

  if (TM_IsStepTimeout()) {
    return TM_TIMEOUT_STEP;
  }

  return TM_TIMEOUT_NONE;
}

/*============================================================================
 *                          软件延时API实现
 *===========================================================================*/

void TM_SetDelay(uint32_t delay_ms) {
  s_tm_state.delay_ms = delay_ms;
  s_tm_state.delay_start_tick = s_tm_state.sys_tick;
  s_tm_state.delay_active = true;

  // log_d("软件延时设置: %lu ms", (unsigned long)delay_ms);
}

bool TM_IsDelayComplete(void) {
  if (!s_tm_state.delay_active) {
    return true; /* 无活跃延时视为已完成 */
  }

  uint32_t elapsed = TM_GetElapsed(s_tm_state.delay_start_tick);
  if (elapsed >= s_tm_state.delay_ms) {
    s_tm_state.delay_active = false; /* 自动清除 */
    return true;
  }
  return false;
}

bool TM_IsDelayActive(void) { return s_tm_state.delay_active; }

void TM_CancelDelay(void) { s_tm_state.delay_active = false; }

uint32_t TM_GetDelayRemaining(void) {
  if (!s_tm_state.delay_active) {
    return 0;
  }

  uint32_t elapsed = TM_GetElapsed(s_tm_state.delay_start_tick);
  if (elapsed >= s_tm_state.delay_ms) {
    return 0;
  }
  return s_tm_state.delay_ms - elapsed;
}

/*============================================================================
 *                          周期任务API实现
 *===========================================================================*/

void TM_StartPeriod(TM_PeriodId_t id, uint32_t interval_ms) {
  if (id >= TM_PERIOD_ID_MAX) {
    return;
  }

  s_tm_state.period_interval[id] = interval_ms;
  s_tm_state.period_last_tick[id] = s_tm_state.sys_tick;
  s_tm_state.period_active[id] = true;

  // log_d("周期任务[%d]启动: 间隔 %lu ms", id, (unsigned long)interval_ms);
}

void TM_StopPeriod(TM_PeriodId_t id) {
  if (id >= TM_PERIOD_ID_MAX) {
    return;
  }

  s_tm_state.period_active[id] = false;
}

bool TM_IsPeriodElapsed(TM_PeriodId_t id) {
  if (id >= TM_PERIOD_ID_MAX || !s_tm_state.period_active[id]) {
    return false;
  }

  uint32_t elapsed = TM_GetElapsed(s_tm_state.period_last_tick[id]);
  if (elapsed >= s_tm_state.period_interval[id]) {
    /* 自动重置计时 */
    s_tm_state.period_last_tick[id] = s_tm_state.sys_tick;
    return true;
  }
  return false;
}

/*============================================================================
 *                          阻塞延时API实现
 *===========================================================================*/

void TM_DelayMs(uint32_t ms) {
  uint32_t start = s_tm_state.sys_tick;
  while (TM_GetElapsed(start) < ms) {
    /* 空循环等待 */
    /* 如果有看门狗，可以在这里喂狗 */
  }
}

void TM_DelayUs(uint32_t us) {
  /* 微秒级延时需要更精确的计时方法 */
  /* 这里使用简单的循环估算，实际精度取决于CPU频率 */
  /* 假设 CPU 频率为 48MHz，每个循环约 6 个周期 */
  volatile uint32_t count = (us * 8); /* 根据实际CPU频率调整 */
  while (count--) {
    __asm volatile("nop");
  }
}

/*============================================================================
 *                          调试API实现
 *===========================================================================*/

const TM_State_t *TM_GetState(void) { return &s_tm_state; }

void TM_PrintStatus(void) {
  // log_i("========== 时间管理器状态 ==========");
  // log_i("系统滴答: %lu ms", (unsigned long)s_tm_state.sys_tick);
  // log_i("全局超时: %s, 剩余 %lu ms",
  //       s_tm_state.global_timeout_active ? "活跃" : "停止",
  //       (unsigned long)TM_GetGlobalRemaining());
  // log_i("单步超时: %s, 剩余 %lu ms",
  //       s_tm_state.step_timeout_active ? "活跃" : "停止",
  //       (unsigned long)TM_GetStepRemaining());
  // log_i("软件延时: %s, 剩余 %lu ms",
  //       s_tm_state.delay_active ? "活跃" : "停止",
  //       (unsigned long)TM_GetDelayRemaining());
  // log_i("=====================================");
}
