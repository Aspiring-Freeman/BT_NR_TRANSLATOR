/**
 * @file pc_protocol_legacy.c
 * @brief 现有PC协议适配层 - 对接 PC_xieyi_Ctrl.c
 * @version 1.0.0
 * @date 2024-12
 *
 * @section intro 简介
 * 这是一个适配层，将现有的 PC_xieyi_Ctrl.c 代码接入 Protocol 模块框架。
 * 不修改原有代码，仅做转发和包装。
 */

#define LOG_TAG "pc_legacy"

#include "pc_protocol.h"
#include <elog.h>
#include <string.h>

/*============ Stub 实现 - 原 PC_xieyi_Ctrl.c 已弃用 ============*/

static void PC_xieyijiexi(uint8_t zufuchua[], uint16_t lenth) {
  log_w("PC_xieyijiexi: legacy function not implemented");
  (void)zufuchua;
  (void)lenth;
}

static void PC_xieyifasong_2(void) {
  log_w("PC_xieyifasong_2: legacy function not implemented");
}

static void PC_xieyifasong_3(void) {
  log_w("PC_xieyifasong_3: legacy function not implemented");
}

/*============ 内部变量 ============*/

static ProtocolSendFunc s_send_func = NULL;
static ProtocolEventCallback s_event_callback = NULL;

/*============ 内部函数声明 ============*/

static bool legacy_init(void);
static ProtocolResult legacy_parse(uint8_t *data, uint16_t len);
static bool legacy_send_cmd(uint16_t cmd, void *param);
static void legacy_on_response(uint16_t code, const uint8_t *data,
                               uint16_t len);
static void legacy_set_send_func(ProtocolSendFunc func);
static void legacy_set_event_callback(ProtocolEventCallback callback);

/*============ 协议接口实例 ============*/

const ProtocolInterface legacy_pc_protocol = {
    .name = "legacy",
    .init = legacy_init,
    .parse = legacy_parse,
    .send_cmd = legacy_send_cmd,
    .on_response = legacy_on_response,
    .set_send_func = legacy_set_send_func,
    .set_event_callback = legacy_set_event_callback,
};

/*============ 接口实现 ============*/

static bool legacy_init(void) {
  log_i("Legacy PC协议适配层初始化 (使用PC_xieyi_Ctrl.c)");
  return true;
}

/**
 * @brief 解析数据 - 直接转发给现有代码
 */
static ProtocolResult legacy_parse(uint8_t *data, uint16_t len) {
  log_d("Legacy: 转发数据到 PC_xieyijiexi(), 长度=%d", len);

  // 直接调用现有的协议解析函数
  PC_xieyijiexi(data, len);

  return PROTOCOL_RESULT_OK;
}

/**
 * @brief 发送命令 - 调用现有的发送函数
 */
static bool legacy_send_cmd(uint16_t cmd, void *param) {
  switch (cmd) {
  case PC_CMD_RESULT_RESPONSE: // 0xAD
    log_d("Legacy: 发送测试结果");
    PC_xieyifasong_2();
    return true;

  case PC_CMD_SET_CONFIG_ACK: // 0xAF
    log_d("Legacy: 发送配置应答");
    PC_xieyifasong_3();
    return true;

  default:
    log_w("Legacy: 未支持的命令 0x%04X", cmd);
    return false;
  }
}

static void legacy_on_response(uint16_t code, const uint8_t *data,
                               uint16_t len) {
  log_d("Legacy: 收到响应 0x%04X", code);
}

static void legacy_set_send_func(ProtocolSendFunc func) {
  s_send_func = func;
  log_d("Legacy: 设置发送函数");
}

static void legacy_set_event_callback(ProtocolEventCallback callback) {
  s_event_callback = callback;
}
