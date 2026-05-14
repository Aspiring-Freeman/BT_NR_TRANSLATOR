/**
 * @file pc_protocol_common.c
 * @brief PC协议公共功能实现
 * @version 1.1.0
 * @date 2026-01
 *
 * @section intro 简介
 * 实现PC协议层的公共功能，如工位号获取、失败信息查询回调等。
 * 用于协议层与应用层（Test_List等）的解耦。
 */

#define LOG_TAG "pc_common"

#include "pc_protocol.h"
#include <elog.h>
#include <string.h>

/*============ 内部变量 ============*/

// 工位号获取回调函数
static PCGetStationIdFunc s_get_station_id_func = NULL;

// 失败信息获取回调函数
static PCGetFailInfoFunc s_get_fail_info_func = NULL;

/*============ API实现 ============*/

/**
 * @brief 设置获取工位号的回调函数
 */
void PC_Protocol_SetStationIdFunc(PCGetStationIdFunc func) {
  s_get_station_id_func = func;
  if (func != NULL) {
    log_i("工位号回调已设置, 当前工位=%d", func());
  } else {
    log_w("工位号回调被清除");
  }
}

/**
 * @brief 获取本机工位号
 */
uint8_t PC_Protocol_GetStationId(void) {
  if (s_get_station_id_func != NULL) {
    return s_get_station_id_func();
  }
  log_w("工位号回调未设置，返回默认值0");
  return 0;
}

// 2个字节，获取当前的工装的程序的版本号
typedef uint16_t (*PCGetVersionFunc)(void);
PCGetVersionFunc pc_get_version_func = NULL;

PCGetVersionFunc PC_Protocol_SetGetVersionFunc(PCGetVersionFunc func) {
  pc_get_version_func = func;
  if (func != NULL) {
    log_i("程序版本回调已设置, 版本=%d", func());
  } else {
    log_w("程序版本回调被清除");
    // 返回默认异常值
    return NULL;
  }
  return pc_get_version_func;
}

PCFTControlFunc pc_ft_control_func = NULL;
PCFTControlFunc PC_Protocol_SetFTControlFunc(PCFTControlFunc func) {
  pc_ft_control_func = func;
  if (func != NULL) {
    log_i("工装控制回调已设置");
  } else {
    log_w("工装控制回调被清除");
    // 返回默认异常值
    return NULL;
  }
  return pc_ft_control_func;
}

PCFTControlFunc PC_Protocol_GetFTControlFunc(void) {
  return pc_ft_control_func;
}

// 6个字节的当前的程序的编译时间
typedef void (*PCGetBuildTimeFunc)(char *build_time);
PCGetBuildTimeFunc pc_get_build_time_func = NULL;

PCGetBuildTimeFunc PC_Protocol_SetGetBuildTimeFunc(PCGetBuildTimeFunc func) {
  pc_get_build_time_func = func;
  if (func != NULL) {
    char build_time[32] = {0};
    func(build_time);
    log_i("编译时间回调已设置, 编译时间=%s", build_time);
  } else {
    log_w("编译时间回调被清除");
    // 返回默认异常值
    return NULL;
  }
  return pc_get_build_time_func;
}

/**
 * @brief 设置获取失败信息的回调函数
 */
void PC_Protocol_SetFailInfoFunc(PCGetFailInfoFunc func) {
  s_get_fail_info_func = func;
  if (func != NULL) {
    log_i("失败信息回调已设置");
  } else {
    log_w("失败信息回调被清除");
  }
}

/**
 * @brief 获取测试失败信息
 */
uint8_t PC_Protocol_GetFailInfo(uint8_t *step_id, char *step_name,
                                uint8_t *fail_reason, char *reason_name) {
  if (s_get_fail_info_func != NULL) {
    return s_get_fail_info_func(step_id, step_name, fail_reason, reason_name);
  }

  // 回调未设置，返回默认值
  if (step_id)
    *step_id = 0;
  if (step_name)
    strcpy(step_name, "UNKNOWN");
  if (fail_reason)
    *fail_reason = 0;
  if (reason_name)
    strcpy(reason_name, "Callback not set");
  log_w("失败信息回调未设置");
  return 0; // 返回进行中状态
}
