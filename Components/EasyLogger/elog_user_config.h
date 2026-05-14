/**
 * @file elog_user_config.h
 * @brief EasyLogger 用户配置
 * @note 修改下面的 true/false 来调整日志行为，无需了解底层细节
 *
 * 日志输出示例:
 *   简洁: "I/main     [00:01.234] 消息内容"
 *   详细: "D/main     [00:01.234] (main.c:123 func_name) 消息内容"
 */

#ifndef __ELOG_USER_CONFIG_H__
#define __ELOG_USER_CONFIG_H__

#include "elog.h"

/*============================================================================
 *                          日志级别配置
 *============================================================================*/
/**
 * 日志过滤级别 (只显示该级别及以上的日志)
 *
 * 可选值:
 *   ELOG_LVL_ASSERT  (0) - 只显示 Assert (最少)
 *   ELOG_LVL_ERROR   (1) - 显示 Assert + Error
 *   ELOG_LVL_WARN    (2) - 显示 Assert + Error + Warn
 *   ELOG_LVL_INFO    (3) - 显示 Assert + Error + Warn + Info (推荐)
 *   ELOG_LVL_DEBUG   (4) - 显示 Assert + Error + Warn + Info + Debug
 *   ELOG_LVL_VERBOSE (5) - 显示全部 (最多)
 */
#define ELOG_USER_FILTER_LVL ELOG_LVL_VERBOSE

/*============================================================================
 *                          输出格式配置 (true/false)
 *============================================================================*/

/* 颜色输出 (需要支持ANSI的终端, 如 PuTTY/MobaXterm/VSCode) */
#define ELOG_SHOW_COLOR true

/* 日志级别标识 (如 I/, D/, E/) */
#define ELOG_SHOW_LEVEL true

/* 日志标签 (如 main, uart, test) */
#define ELOG_SHOW_TAG true

/* 时间戳 (如 [00:01.234]) */
#define ELOG_SHOW_TIME true

/* 文件名 (如 main.c) - 仅 Debug/Verbose 级别 */
#define ELOG_SHOW_FILE false

/* 函数名 (如 Loop_NormalTest) - 仅 Debug/Verbose 级别 */
#define ELOG_SHOW_FUNC false

/* 行号 (如 :123) - 仅 Debug/Verbose 级别 */
#define ELOG_SHOW_LINE false

/*============================================================================
 *                          内部实现 (不需要修改)
 *============================================================================*/

/* 构建格式掩码 */
#define _ELOG_FMT_BASIC                                                        \
  ((ELOG_SHOW_LEVEL ? ELOG_FMT_LVL : 0) | (ELOG_SHOW_TAG ? ELOG_FMT_TAG : 0) | \
   (ELOG_SHOW_TIME ? ELOG_FMT_TIME : 0))

#define _ELOG_FMT_DEBUG                                                        \
  (_ELOG_FMT_BASIC | (ELOG_SHOW_FILE ? ELOG_FMT_DIR : 0) |                     \
   (ELOG_SHOW_FUNC ? ELOG_FMT_FUNC : 0) |                                      \
   (ELOG_SHOW_LINE ? ELOG_FMT_LINE : 0))

/**
 * @brief 应用用户配置并启动 EasyLogger
 */
static inline void ELog_UserInit(void) {
  elog_init();

  /* Assert: 显示全部信息 (严重错误时需要完整上下文) */
  elog_set_fmt(ELOG_LVL_ASSERT,
               ELOG_FMT_ALL & ~ELOG_FMT_P_INFO & ~ELOG_FMT_T_INFO);

  /* Error/Warn/Info: 使用基本格式 (简洁) */
  elog_set_fmt(ELOG_LVL_ERROR, _ELOG_FMT_BASIC);
  elog_set_fmt(ELOG_LVL_WARN, _ELOG_FMT_BASIC);
  elog_set_fmt(ELOG_LVL_INFO, _ELOG_FMT_BASIC);

  /* Debug: 使用详细格式 (带文件名/函数名/行号) */
  elog_set_fmt(ELOG_LVL_DEBUG, _ELOG_FMT_DEBUG);

  /* Verbose: 使用详细格式 */
  elog_set_fmt(ELOG_LVL_VERBOSE, _ELOG_FMT_DEBUG);

  /* 应用用户配置 */
  elog_set_filter_lvl(ELOG_USER_FILTER_LVL);
  elog_set_text_color_enabled(ELOG_SHOW_COLOR);

  elog_start();
}

#endif /* __ELOG_USER_CONFIG_H__ */
