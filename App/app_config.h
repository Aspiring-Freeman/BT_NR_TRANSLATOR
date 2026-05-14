/**
 * @file    app_config.h
 * @brief   应用层配置 (BT_PassThroughFT)
 *
 * 版本号由此文件定义，协议层可直接包含。
 * main.c 同步定义 SOFTWARE_VERSION_* 以供 CMakeLists.txt 正则提取。
 */
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

/*============================================================================
 *  版本号 — CMakeLists.txt 用正则从本文件提取下面三行的字面量
 *  若需修改版本，同步更新 app_config.h 中的 APP_VERSION_*
 *===========================================================================*/
#define SOFTWARE_VERSION_MAJOR 1
#define SOFTWARE_VERSION_MINOR 0
#define SOFTWARE_VERSION_PATCH 7
/* 版本字符串自动由上面三个数字拼成,改版本只需改 MAJOR/MINOR/PATCH */
#define _SW_VER_STR(x) #x
#define _SW_VER_XSTR(x) _SW_VER_STR(x)
#define SOFTWARE_VERSION_STRING                                                \
  "v" _SW_VER_XSTR(SOFTWARE_VERSION_MAJOR) "." _SW_VER_XSTR(                   \
      SOFTWARE_VERSION_MINOR) "." _SW_VER_XSTR(SOFTWARE_VERSION_PATCH)

/* 调试模式配置 */
// #define ENABLE_WATCHDOG // 取消注释以启用看门狗

/* 日志配置 */
#define DEBUG_PRINT_TIME 10000 // 调试打印时间间隔 (ms)

/*============ 功能开关 ============*/
/* #define ENABLE_WATCHDOG */   ///< 使能看门狗
/* #define ENABLE_SLEEP_MODE */ ///< 使能低功耗睡眠

#endif /* __APP_CONFIG_H__ */
