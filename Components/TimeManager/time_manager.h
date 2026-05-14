/**
 * @file time_manager.h
 * @brief 统一时间管理模块 - 集中管理所有时间配置
 * @details 提供超时、延时、周期任务的统一管理接口
 *          所有时间相关的配置集中在此文件，便于维护和修改
 *
 * @version 2.0.0
 * @date 2026-01-30
 *
 * 使用说明：
 * =========
 * 1. 在 time.c 的定时器中断中调用 TM_SysTick_Handler()
 * 2. 在 main.c 初始化时调用 TM_Init()
 * 3. 使用 TM_SetStepTimeout() / TM_IsStepTimeout() 管理步骤超时
 * 4. 使用 TM_SetDelay() / TM_IsDelayComplete() 管理非阻塞延时
 * 5. 使用 TM_StartGlobalTimeout() / TM_CheckTimeout() 管理全局超时
 */

#ifndef __TIME_MANAGER_H__
#define __TIME_MANAGER_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 *                          版本信息
 *===========================================================================*/
#define TIME_MANAGER_VERSION_MAJOR 2
#define TIME_MANAGER_VERSION_MINOR 0
#define TIME_MANAGER_VERSION_PATCH 0

/*============================================================================
 *                    时间配置宏定义 - 集中管理所有超时值
 *============================================================================
 * 命名规则：
 *   TM_TIMEOUT_xxx  - 超时时间(ms)
 *   TM_DELAY_xxx    - 延时时间(ms)
 *   TM_PERIOD_xxx   - 周期时间(ms)
 *===========================================================================*/

/*---------------------------------------------------------------------------
 *                          全局超时配置
 *---------------------------------------------------------------------------*/
/** @brief 测试总超时时间 (100秒) */
#define TM_TIMEOUT_GLOBAL_TEST 100000

/** @brief 上电无响应最大等待时间 (5分钟) */
#define TM_TIMEOUT_POWER_ON_NO_RESP 300000

/*---------------------------------------------------------------------------
 *                          测试结束后供电等待配置
 *---------------------------------------------------------------------------*/
/** @brief 测试成功后保持供电时间 (5分钟) - 供工人按压按钮 */
#define TM_DELAY_SUCCESS_KEEP_POWER 300000

/** @brief 测试失败后保持供电时间 (0=立即断电, 调试时可改为300000) */
#define TM_DELAY_FAIL_KEEP_POWER 0

/*---------------------------------------------------------------------------
 *                          步骤超时配置 - 膜式燃气表测试
 *---------------------------------------------------------------------------*/
/** @brief 协议响应超时 (5秒) - 通用协议响应等待 */
#define TM_TIMEOUT_PROTOCOL_RESP 5000

/** @brief 等待自检完成超时 (10秒) */
#define TM_TIMEOUT_SELF_CHECK_WAIT 10000

/** @brief 等待板级信息超时 (10秒) */
#define TM_TIMEOUT_BOARD_INFO_WAIT 10000

/** @brief 电压检测超时 (2秒) */
#define TM_TIMEOUT_VOLTAGE_CHECK 2000

/** @brief 功耗检测超时 (5秒) */
#define TM_TIMEOUT_POWER_CHECK 5000

/** @brief 阀门测试超时 (10秒) */
#define TM_TIMEOUT_VALVE_TEST 10000

/** @brief 阀门到位检测超时 (15秒) */
#define TM_TIMEOUT_VALVE_POSITION 15000

/** @brief 温度传感器检测超时 (3秒) */
#define TM_TIMEOUT_TEMP_CHECK 3000

/** @brief NB连接超时 (30秒) */
#define TM_TIMEOUT_NB_CONNECT 30000

/** @brief IMEI读取超时 (5秒) */
#define TM_TIMEOUT_IMEI_READ 5000

/** @brief 红外关闭超时 (3秒) */
#define TM_TIMEOUT_IR_CLOSE 3000

/** @brief 蓝牙测试超时 (10秒) */
#define TM_TIMEOUT_BLUETOOTH_TEST 10000

/** @brief IC模块测试超时 (5秒) */
#define TM_TIMEOUT_IC_MODULE_TEST 5000

/** @brief 计量模块测试超时 (5秒) */
#define TM_TIMEOUT_METERING_TEST 5000

/*---------------------------------------------------------------------------
 *                          延时配置
 *---------------------------------------------------------------------------*/
/** @brief 电源稳定延时 (100ms) */
#define TM_DELAY_POWER_STABLE 100

/** @brief 电压测量稳定延时 (50ms) */
#define TM_DELAY_VOLTAGE_STABLE 50

/** @brief 电流测量稳定延时 (500ms) */
#define TM_DELAY_CURRENT_STABLE 500

/** @brief ADC采样稳定延时 (10ms) */
#define TM_DELAY_ADC_STABLE 10

/** @brief 阀门动作延时 (500ms) */
#define TM_DELAY_VALVE_ACTION 500

/** @brief 重试间隔延时 (200ms) */
#define TM_DELAY_RETRY_INTERVAL 200

/** @brief 协议发送间隔 (100ms) */
#define TM_DELAY_PROTOCOL_INTERVAL 100

/** @brief 测试成功后断电延时 (500ms) */
#define TM_DELAY_SUCCESS_POWEROFF 500

/** @brief 短延时 (50ms) */
#define TM_DELAY_SHORT 50

/** @brief 中延时 (200ms) */
#define TM_DELAY_MEDIUM 200

/** @brief 长延时 (500ms) */
#define TM_DELAY_LONG 500

/*---------------------------------------------------------------------------
 *                          周期任务配置
 *---------------------------------------------------------------------------*/
/** @brief 调试打印周期 (2000ms) */
#define TM_PERIOD_DEBUG_PRINT 2000

/** @brief 功耗检测周期 (500ms) */
#define TM_PERIOD_POWER_CHECK 500

/** @brief LED闪烁周期 (500ms) */
#define TM_PERIOD_LED_BLINK 500

/** @brief 看门狗喂狗周期 (1000ms) */
#define TM_PERIOD_WATCHDOG 1000

/*============================================================================
 *                          周期任务ID定义
 *============================================================================*/

/**
 * @brief 周期任务ID枚举
 */
typedef enum {
  TM_PERIOD_ID_LED = 0,  /**< LED闪烁周期任务 */
  TM_PERIOD_ID_POWER,    /**< 功耗检测周期任务 */
  TM_PERIOD_ID_DEBUG,    /**< 调试打印周期任务 */
  TM_PERIOD_ID_WATCHDOG, /**< 看门狗喂狗周期任务 */
  TM_PERIOD_ID_USER1,    /**< 用户自定义周期任务1 */
  TM_PERIOD_ID_USER2,    /**< 用户自定义周期任务2 */
  TM_PERIOD_ID_MAX       /**< 周期任务最大数量 */
} TM_PeriodId_t;

/*============================================================================
 *                          超时结果枚举
 *============================================================================*/

/**
 * @brief 超时检测结果枚举
 */
typedef enum {
  TM_TIMEOUT_NONE = 0, /**< 无超时 */
  TM_TIMEOUT_GLOBAL,   /**< 全局超时 */
  TM_TIMEOUT_STEP,     /**< 单步超时 */
} TM_TimeoutResult_t;

/*============================================================================
 *                          内部状态结构体（调试用）
 *============================================================================*/

/**
 * @brief 时间管理器内部状态结构体
 * @note 主要用于调试和状态查询
 */
typedef struct {
  /* 系统滴答 */
  volatile uint32_t sys_tick; /**< 系统毫秒计数 */

  /* 全局超时 */
  uint32_t global_timeout_ms; /**< 全局超时时间 */
  uint32_t global_start_tick; /**< 全局超时起始Tick */
  bool global_timeout_active; /**< 全局超时是否活跃 */

  /* 单步超时 */
  uint32_t step_timeout_ms; /**< 单步超时时间 */
  uint32_t step_start_tick; /**< 单步超时起始Tick */
  bool step_timeout_active; /**< 单步超时是否活跃 */

  /* 软件延时 */
  uint32_t delay_ms;         /**< 延时时间 */
  uint32_t delay_start_tick; /**< 延时起始Tick */
  bool delay_active;         /**< 延时是否活跃 */

  /* 周期任务 */
  uint32_t period_interval[TM_PERIOD_ID_MAX]; /**< 各周期任务间隔 */
  uint32_t period_last_tick[TM_PERIOD_ID_MAX]; /**< 各周期任务上次触发Tick */
  bool period_active[TM_PERIOD_ID_MAX]; /**< 各周期任务是否活跃 */
} TM_State_t;

/*============================================================================
 *                          核心API函数
 *============================================================================*/

/**
 * @brief 初始化时间管理器
 * @note 必须在系统启动时调用一次
 */
void TM_Init(void);

/**
 * @brief 时间管理器1ms中断处理
 * @note 必须在1ms定时器中断中调用
 */
void TM_SysTick_Handler(void);

/**
 * @brief 获取系统运行时间(ms)
 * @return 系统启动后的毫秒数
 */
uint32_t TM_GetTick(void);

/**
 * @brief 计算从指定时刻到现在的时间差(ms)
 * @param start_tick 起始时刻
 * @return 经过的毫秒数
 */
uint32_t TM_GetElapsed(uint32_t start_tick);

/*============================================================================
 *                          全局超时API
 *============================================================================*/

/**
 * @brief 启动全局超时计时
 * @param timeout_ms 超时时间(ms)，0使用默认值 TM_TIMEOUT_GLOBAL_TEST
 */
void TM_StartGlobalTimeout(uint32_t timeout_ms);

/**
 * @brief 停止全局超时计时
 */
void TM_StopGlobalTimeout(void);

/**
 * @brief 检查全局超时是否已触发
 * @return true=超时, false=未超时
 */
bool TM_IsGlobalTimeout(void);

/**
 * @brief 获取全局超时剩余时间(ms)
 * @return 剩余毫秒数，0表示已超时或未启动
 */
uint32_t TM_GetGlobalRemaining(void);

/*============================================================================
 *                          单步超时API
 *============================================================================*/

/**
 * @brief 设置单步超时时间
 * @param timeout_ms 超时时间(ms)
 */
void TM_SetStepTimeout(uint32_t timeout_ms);

/**
 * @brief 重置单步超时计时（使用当前超时值重新计时）
 */
void TM_ResetStepTimeout(void);

/**
 * @brief 停止单步超时计时
 */
void TM_StopStepTimeout(void);

/**
 * @brief 检查单步超时是否已触发
 * @return true=超时, false=未超时
 */
bool TM_IsStepTimeout(void);

/**
 * @brief 获取单步超时剩余时间(ms)
 * @return 剩余毫秒数
 */
uint32_t TM_GetStepRemaining(void);

/*============================================================================
 *                          统一超时检查API
 *============================================================================*/

/**
 * @brief 统一检查超时状态（推荐使用）
 * @return 超时类型枚举
 * @note 在主循环开始处调用，统一处理超时
 */
TM_TimeoutResult_t TM_CheckTimeout(void);

/*============================================================================
 *                          非阻塞延时API
 *============================================================================*/

/**
 * @brief 设置非阻塞延时
 * @param delay_ms 延时时间(ms)
 */
void TM_SetDelay(uint32_t delay_ms);

/**
 * @brief 检查非阻塞延时是否完成
 * @return true=完成, false=进行中
 */
bool TM_IsDelayComplete(void);

/**
 * @brief 检查延时是否正在进行中
 * @return true=进行中, false=未激活
 */
bool TM_IsDelayActive(void);

/**
 * @brief 取消当前延时
 */
void TM_CancelDelay(void);

/**
 * @brief 获取延时剩余时间(ms)
 * @return 剩余毫秒数
 */
uint32_t TM_GetDelayRemaining(void);

/*============================================================================
 *                          周期任务API
 *============================================================================*/

/**
 * @brief 启动周期任务
 * @param id 任务ID
 * @param interval_ms 周期时间(ms)
 */
void TM_StartPeriod(TM_PeriodId_t id, uint32_t interval_ms);

/**
 * @brief 停止周期任务
 * @param id 任务ID
 */
void TM_StopPeriod(TM_PeriodId_t id);

/**
 * @brief 检查周期任务是否到期（到期后自动重置）
 * @param id 任务ID
 * @return true=到期, false=未到期
 */
bool TM_IsPeriodElapsed(TM_PeriodId_t id);

/*============================================================================
 *                          阻塞延时API（谨慎使用）
 *============================================================================*/

/**
 * @brief 阻塞延时 - 毫秒级
 * @param ms 延时时间(ms)
 * @warning 会阻塞整个系统，建议只在初始化阶段使用
 */
void TM_DelayMs(uint32_t ms);

/**
 * @brief 阻塞延时 - 微秒级
 * @param us 延时时间(us)
 * @warning 精度取决于CPU频率，仅作估算
 */
void TM_DelayUs(uint32_t us);

/*============================================================================
 *                          调试API
 *============================================================================*/

/**
 * @brief 获取时间管理器内部状态（只读）
 * @return 状态结构体指针
 */
const TM_State_t *TM_GetState(void);

/**
 * @brief 打印时间管理器状态（调试用）
 */
void TM_PrintStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* __TIME_MANAGER_H__ */
