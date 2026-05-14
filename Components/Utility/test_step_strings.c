/**
 * @file test_step_strings.c
 * @brief 测试步骤名称字符串库实现
 */

#include "test_step_strings.h"
#include <stddef.h>

// 步骤ID对应的中文名称 (与 Test_liucheng 枚举一一对应)
static const char *const s_step_names[] = {
    [0] = "等待开始",          // w_wait
    [1] = "开始测试",          // w_start
    [2] = "设置累计流量",      // w_reset_accumulated_flux
    [3] = "关阀等待响应",      // w_close_valve_wait_response
    [4] = "切换空水状态",      // w_qiehuan_kongshui
    [5] = "功耗测试",          // w_gonghao_CHK
    [6] = "切换满水状态",      // w_qiehuan_manshui
    [7] = "满水功耗测试",      // w_gonghao_manshui_CHK
    [8] = "切换走水状态",      // w_qiehuan_zoushui
    [9] = "走水功耗测试",      // w_gonghao_zoushui_CHK
    [10] = "读取累计流量",     // w_read_accumulated_flux
    [11] = "电磁铁唤醒红外",   // w_diancitie_huanxing_irda
    [12] = "获取表号",         // get_biaohao
    [13] = "获取版本号",       // get_banbenhao
    [14] = "设置阀门配置",     // set_famen_peizhi
    [15] = "主动上报",         // w_zhudong_shangg
    [16] = "查询上报结果开始", // w_shangg_jieguo_start
    [17] = "查询上报结果",     // w_shangg_jieguo
    [18] = "阀门测试",         // w_famenceshi
    [19] = "阀门测试子流程",   // w_valve_test_process_1
    [20] = "阀门输出电压检测", // w_valve_output_voltage_check
    [21] = "水温检测",         // w_water_temp_CHK
    [22] = "测试结束",         // w_end
};

#define STEP_COUNT (sizeof(s_step_names) / sizeof(s_step_names[0]))

const char *TestStep_GetName(uint8_t step_id) {
  if (step_id < STEP_COUNT && s_step_names[step_id] != NULL) {
    return s_step_names[step_id];
  }
  return "未知步骤";
}

const char *TestStep_GetShortName(uint8_t step_id) {
  // 简短名称直接复用中文名称
  return TestStep_GetName(step_id);
}
