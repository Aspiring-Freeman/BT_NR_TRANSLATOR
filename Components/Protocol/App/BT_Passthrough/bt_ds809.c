/**
 * @file    bt_ds809.c
 * @brief   对公司DS809定制蓝牙模组进行解析
 *
 */
#include "bt_ds809.h"
#include <stdbool.h>
#include <string.h>

#define LOG_TAG "device_protocol_dgm"

#include "utility.h"
#include <elog.h>
#include <stdio.h>
#include <string.h>

extern void protocol_debug_print(const uint8_t *data, uint16_t len);

/*============ 协议帧索引定义 ============*/

#define INDEX_BLE_FRAME1 0  // 第一个蓝牙通信的起始索引 FC
#define INDEX_BLE_ADDR 1    //  地址索引
#define INDEX_BLE_FRAME2 2  // 第二个蓝牙通信的起始索引
#define INDEX_FRAME_TYPE 3  // 报文类型
#define INDEX_DATA_LENGTH 4 // 数据长度起始索引（2字节，小端）
#define INDEX_DATA_MARK 5   // 数据标识索引（2字节，小端）
#define INDEX_VOLUME_DATA 7 // 数据域起始索引

#define DATA_CMD_LENGTH_FRONT 7 // 数据区命令长度前面固定部分

/*============ 控制码定义  ============*/

// 蓝牙模块请求控制码
#define MODULE_QUERY_CODE 0x11
// 蓝牙模块请求，mcu响应控制码
#define MCU_RESPONSE_CODE 0x91
// mcu 请求读取数据控制码
#define MCU_READ_CODE 0x12
// 蓝牙模组响应mcu读取控制码
#define MODULE_READ_RESPONSE_CODE 0x92
//   mcu设置蓝牙模组控制码
#define MCU_SET_CODE 0x13
// 蓝牙模块响应mcu设置控制码
#define MODULE_SET_RESPONSE_CODE 0x93

// 控制码辅助宏
#define IS_RESPONSE(ctrl) ((ctrl) & 0x80) // 检查是否是响应 (D7=1)
#define IS_ABNORMAL(ctrl) ((ctrl) & 0x40) // 检查是否异常应答 (D6=1)

/*============ 数据标识定义 (与PIC一致) ============*/

// 模组请求mcu
#define BLE_CONFIG_WORK_PARAM                                                  \
  0xB007 // 配置工作参数，模组向主控 MCU 请求 MAC 地址、PSW 配对密码、Name
         // 广播设备名称等参数
#define BLE_CONFIG_UUID_PARAM                                                  \
  0xB00D // 配置UUID参数，模组向主控 MCU 请求服务和特征的UUID参数

// mcu 请求读取模组相关数据
#define BLE_SYS_STATUS 0xB000                      // 设置系统状态
#define BLE_READ_BROADCAST_SELF_DEFINE_DATA 0xB008 // 读取广播自定义数据

// mcu 读取模组参数
#define BLE_READ_FIRMWARE_VERSION 0xB011   // 读取固件版本
#define BLE_READ_MAC 0xC001                // 读取蓝牙MAC
#define BLE_READ_DIAMIC_UPDATE_SCAN 0xB015 // 读取动态更新扫描响应数据
#define BLE_READ_CURRENT_CONNECT_STATUS 0xB00E // 读取当前模组连接状态

// mcu设置模组的相关数据
#define BLE_SET_BROADCAST_PARAMS 0xB012      // 设置广播参数
#define BLE_SET_BROADCAST_EMMIT_POWER 0xB013 // 设置发射功率
#define BLE_SET_BROADCAST_UPDATE_DATA                                          \
  0xB014 // 设置动态更新广播数据，慎用，不要发这个，使用默认的蓝牙协议就好
#define BLE_SET_BLUETOOTH_SYSTEM_STATUS 0xB000 // 设置蓝牙系统状态

// 上位机测试命令（已移除，相关定义请在上位机代码中维护）

/*============ 设备类型定义 ============*/

#define DEVICE_TYPE 0x08 // RTU类型

/*============ 检测过程标志定义 (与PIC一致) ============*/

#define MASTER_HALT 0
#define MASTER_SELFCHECK_FINISH 1
#define MASTER_CONNCET_CHECK 2
#define MASTER_CHECK_ONE 3
#define MASTER_CHECK_TWO 4
#define MASTER_CHECK_CURRENT 5
#define MASTER_CHECK_END 6
#define MASTER_IR_CLOSED 7

/*============ 内部变量 ============*/

static ProtocolSendFunc s_send_func = NULL;
static ProtocolEventCallback s_event_callback = NULL;

// 膜式燃气表专用事件回调 (推荐使用)
static BleEventCallback s_ble_event_callback = NULL;

// 发送缓冲区
#define DGM_TX_BUF_SIZE 256
static uint8_t s_tx_buffer[DGM_TX_BUF_SIZE];

/*============ 内部函数声明 ============*/

static bool dgm_init(void);
static ProtocolResult ble_parse(uint8_t *data, uint16_t len);
static bool ble_send_cmd(uint16_t cmd, void *param);
static void dgm_on_response(uint16_t code, const uint8_t *data, uint16_t len);
static void dgm_set_send_func(ProtocolSendFunc func);
static void dgm_set_event_callback(ProtocolEventCallback callback);

// 响应处理函数
static void handle_module_query(const uint8_t *frame, uint16_t data_mark,
                                uint16_t data_len);
static void handle_old_version_response(const uint8_t *frame,
                                        uint16_t data_mark, uint16_t data_len);
static void handle_read_response(const uint8_t *frame, uint16_t data_mark,
                                 uint16_t data_len);
static void handle_write_response(const uint8_t *frame, uint16_t data_mark);

// 命令发送函数
static uint16_t build_cmd_frame(uint8_t *buf, uint8_t ctrl_code,
                                uint16_t data_mark, const uint8_t *data,
                                uint16_t data_len);
static bool send_read_cmd(uint16_t data_mark);
static bool send_write_cmd(uint16_t data_mark, const uint8_t *data,
                           uint16_t data_len);

/*============ 协议接口实例 ============*/

const ProtocolInterface diaphragm_gas_meter_protocol = {
    .name = "diaphragm_gas_meter",
    .init = dgm_init,
    .parse = ble_parse,
    .send_cmd = ble_send_cmd,
    .on_response = dgm_on_response,
    .set_send_func = dgm_set_send_func,
    .set_event_callback = dgm_set_event_callback,
    .preamble = NULL, // 膜式燃气表不需要前导
};

/*============ 接口实现 ============*/

static bool dgm_init(void) {
  log_i("蓝牙 下位机协议初始化");
  return true;
}

/**
 * @brief 解析主控板响应
 *
 * 协议帧格式与上位机协议相同
 */
static ProtocolResult ble_parse(uint8_t *data, uint16_t len) {
  uint16_t pos = 0;
  bool handled = false;

  log_d("蓝牙协议解析, 长度=%d", len);
  elog_hexdump("DGM_RX", 8, data, len);

  // 最小前缀长度: 9字节 (能读到数据域长度字段)
  // 帧头1(1) + 地址(1) + 帧头2(1) + 控制码(1) + 数据长度(1) + 数据标识(2) +
  // 数据域至少0字节+ 校验和(1) + 帧尾(1) = 9字节
  while (pos + 9 <= len) {
    // 查找第一个帧头,如果不对，先结束本次循环解析，继续下一个循环
    if (data[pos + INDEX_BLE_FRAME1] != FRAME_HEAD_BLE) {
      pos++;
      continue;
    }

    // 验证第二个帧头
    if (data[pos + INDEX_BLE_FRAME2] != FRAME_HEAD_BLE) {
      pos++;
      continue;
    }

    // 获取数据域长度 (单字节)
    uint16_t data_field_len = data[pos + INDEX_DATA_LENGTH];

    // 验证数据域长度的合理性
    // 不需要验证，当前最小数据域可能是0

    //  最大数据域长度限制
    //  (防止异常数据),蓝牙协议定义的数据域最大长度是253字节，所以这里设置一个合理的上限
    if (data_field_len > 253) {
      log_w("数据域长度过大: %d", data_field_len);
      pos++;
      continue;
    }

    // 计算完整帧长度: 前缀(5) + 数据域 + 校验和(1) + 帧尾(1)
    uint16_t frame_len = 5 + data_field_len + 2;
    // 验证是否有完整的帧
    if (pos + frame_len > len) {
      log_d("帧不完整, 需要%d字节, 当前有%d字节", frame_len, len - pos);
      return PROTOCOL_RESULT_INCOMPLETE;
    }

    // 验证帧尾
    if (data[pos + frame_len - 1] != FRAME_TAIL_BLE) {
      log_d("帧尾错误: 0x%02X, 期望0x16", data[pos + frame_len - 1]);
      pos++;
      continue;
    }

    // 验证校验和
    uint8_t recv_checksum = data[pos + frame_len - 2];
    uint8_t calc_sum = util_checksum_sum8(&data[pos], frame_len - 2);
    if (recv_checksum != calc_sum) {
      log_e("校验和错误: 计算=0x%02X, 接收=0x%02X", calc_sum, recv_checksum);
      pos++;
      continue;
    }

    // 获取控制码和数据标识
    uint8_t ctrl_code = data[pos + INDEX_FRAME_TYPE];
    uint16_t data_mark = READ_LE_U16(&data[pos + INDEX_DATA_MARK]);

    log_d("收到帧: 控制码=0x%02X, 数据标识=0x%04X", ctrl_code, data_mark);

    // 0x11 是模组主动请求 (B007/B00D), 不是 MCU 的响应; 但需要 MCU 回复
    // 其它 0x9x 是模组对 MCU 命令的响应; 此处统一放行, 由 switch 分发
    if (ctrl_code != MODULE_QUERY_CODE && !IS_RESPONSE(ctrl_code)) {
      log_w("非法控制码 (既不是模组请求也不是响应): 0x%02X", ctrl_code);
      pos++;
      continue;
    }

    // 检查是否异常应答 (D6=1表示异常)
    if (IS_ABNORMAL(ctrl_code)) {
      log_e("收到异常应答: 控制码=0x%02X, 数据标识=0x%04X", ctrl_code,
            data_mark);
      // 触发异常事件
      BleProtocolEvent event = {0};
      event.type = BLE_EVENT_PARSE_ERROR_EVENT;
      event.data_mark = data_mark;
      if (s_ble_event_callback) {
        s_ble_event_callback(&event);
      }
      pos += frame_len;
      continue;
    }

    // 根据响应码分发处理
    switch (ctrl_code) {
    case MODULE_QUERY_CODE: // 0x11 - 蓝牙模组主动发起
      handle_module_query(&data[pos], data_mark, data_field_len);
      handled = true;
      break;

    case MODULE_READ_RESPONSE_CODE: // 0x92 -  读响应
      handle_read_response(&data[pos], data_mark, data_field_len);
      handled = true;
      break;

    case MODULE_SET_RESPONSE_CODE: // 0x93 - 写响应
      handle_write_response(&data[pos], data_mark);
      handled = true;
      break;

    case 0x81: // 这个是设置广播的老的协议响应，单独直接列出
      handle_old_version_response(&data[pos], data_mark, data_field_len);
      handled = true;
      break;

    default:
      log_w("未知响应码: 0x%02X", ctrl_code);
      break;
    }

    pos += frame_len;
  }

  return handled ? PROTOCOL_RESULT_OK : PROTOCOL_RESULT_UNKNOWN_CMD;
}

static bool ble_send_cmd(uint16_t cmd, void *param) {
  // cmd的高8位表示操作类型，低8位是数据标识高字节
  // 或者直接使用预定义的命令码
  switch (cmd) {

    // 设置命令
  case BLE_SET_BROADCAST_PARAMS:
    return send_write_cmd(BLE_SET_BROADCAST_PARAMS, (uint8_t *)param,
                          3); // 配置广播参数

  case BLE_SET_BROADCAST_EMMIT_POWER:
    return send_write_cmd(BLE_SET_BROADCAST_EMMIT_POWER, (uint8_t *)param, 1);

  // 这条协议暂时不要发，保持默认的广播数据就好，避免导致广播异常，模组无法连接，这里如果app有需求定义ltv,那是另外一回事
  // case BLE_SET_BROADCAST_UPDATE_DATA:
  // return send_write_cmd(BLE_SET_BROADCAST_UPDATE_DATA, (uint8_t *)param,
  // 0);
  case BLE_SET_BLUETOOTH_SYSTEM_STATUS:
    return send_write_cmd(BLE_SET_BLUETOOTH_SYSTEM_STATUS, (uint8_t *)param, 1);

  // 读取命令
  case BLE_READ_FIRMWARE_VERSION:
    return send_read_cmd(BLE_READ_FIRMWARE_VERSION);
  case BLE_READ_MAC:
    return send_read_cmd(BLE_READ_MAC);
  case BLE_READ_DIAMIC_UPDATE_SCAN:
    return send_read_cmd(BLE_READ_DIAMIC_UPDATE_SCAN);
  case BLE_READ_CURRENT_CONNECT_STATUS:
    return send_read_cmd(BLE_READ_CURRENT_CONNECT_STATUS);
  default:
    // 默认作为读命令发送
    log_i("当前发送默认命令: 0x%04X", cmd);
    return send_read_cmd(cmd);
  }
}

static void dgm_on_response(uint16_t code, const uint8_t *data, uint16_t len) {
  log_d("膜表协议响应: 0x%04X", code);
}

static void dgm_set_send_func(ProtocolSendFunc func) { s_send_func = func; }

static void dgm_set_event_callback(ProtocolEventCallback callback) {
  s_event_callback = callback;
}

/*============ 响应处理实现 ============*/
/**
 * @brief 蓝牙模组主动发起请求的响应处理
 *
 */
static void handle_module_query(const uint8_t *frame, uint16_t data_mark,
                                uint16_t data_len) {
  const uint8_t *payload = &frame[INDEX_VOLUME_DATA];
  BleProtocolEvent event = {0};

  log_d("蓝牙主动发起请求的响应: 数据标识=0x%04X", data_mark);

  switch (data_mark) {
  case BLE_CONFIG_WORK_PARAM: // B007,蓝牙请求配置工作参数的响应，这个是模组主动发起的请求，mcu需要响应这个请求，告诉模组当前的MAC地址、配对密码和广播设备名称等参数
    event.type = BLE_EVENT_CONFIGURE_BLUETOOTH_EVENT;
    event.data_mark = data_mark;
    // 主动请求的数据域是空的
    log_d("蓝牙模组请求设置系统状态：数据标识=0x%04X", data_mark);
    // 触发事件回调
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;

  case BLE_CONFIG_UUID_PARAM: //
    // 0xB00D，但是在我们当前的读取接口中，这个是老的协议，没有办法
    log_d("蓝牙模组请求配置UUID：数据标识=0x%04X", data_mark);
    event.type = BLE_EVENT_CONFIGURE_UUID_EVENT;
    event.data_mark = data_mark;
    // 解析当前设置返回的广播自定义数据块状态
    // 当前返回的数据没有任何数据，不用管
    //  触发事件回调
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;
  default:
    log_d("未处理的读响应: 0x%04X", data_mark);
    break;
  }
}

/**
 * @brief
 * 处理读响应,处理0x81响应，这个是设置广播数据状态的老的协议，和当前的任何协议不兼容
 */
static void handle_old_version_response(const uint8_t *frame,
                                        uint16_t data_mark, uint16_t data_len) {
  const uint8_t *payload = &frame[INDEX_VOLUME_DATA];
  BleProtocolEvent event = {0};

  log_d("处理例外的响应，老的协议设置广播块的协议: 数据标识=0x%04X", data_mark);

  switch (data_mark) {

  case BLE_READ_BROADCAST_SELF_DEFINE_DATA: // 实际是设置广播自定义数据的响应
    // 0xB008，但是在我们当前的读取接口中，这个是老的协议，没有办法
    log_d("设置广播块数据的响应，老的协议，数据标识=0x%04X", data_mark);
    event.type = BLE_EVENT_SET_BROADCASET_SELF_DEFINE_DATA_EVENT;
    event.data_mark = data_mark;
    // 解析当前设置返回的广播自定义数据块状态
    // 当前返回的数据没有任何数据，不用管
    //  触发事件回调
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;

  default:
    log_d("未处理的读响应: 0x%04X", data_mark);
    break;
  }
}

/**
 * @brief 处理读响应,处理0x92的读取响应
 *@note
 *读取的协议响应支持：读取当前蓝牙是否连接，读取蓝牙MAC地址，读取固件版本，读取动态更新扫描响应数据的响应
 */
static void handle_read_response(const uint8_t *frame, uint16_t data_mark,
                                 uint16_t data_len) {
  const uint8_t *payload = &frame[INDEX_VOLUME_DATA];
  BleProtocolEvent event = {0};

  log_d("处理读响应: 数据标识=0x%04X", data_mark);

  switch (data_mark) {

  case BLE_READ_CURRENT_CONNECT_STATUS: // 读取当前连接状态的响应 0xB00E

    event.type = BLE_EVENT_GET_CURRENT_IF_CONNECTED_EVENT;
    event.data_mark = data_mark;
    // 解析当前设置返回的广播自定义数据块状态
    event.data.get_connection_status_response.connected = payload[0];
    log_i("读取当前连接状态响应: %s",
          event.data.get_connection_status_response.connected ? "已连接"
                                                              : "未连接");
    // 触发事件回调
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;
  case BLE_READ_DIAMIC_UPDATE_SCAN:
    event.type = BLE_EVENT_DYAMIC_UPDATE_SCAN_RESPONSE_DATA_EVENT;
    event.data_mark = data_mark;
    event.data.dynamic_update_scan_response_data_response.update_status =
        payload[0];
    log_i("读取动态更新扫描响应: 数据：%d,状态：%s",
          event.data.dynamic_update_scan_response_data_response.update_status,
          event.data.dynamic_update_scan_response_data_response.update_status
              ? "设置失败"
              : "设置成功");
    // 触发回调
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;
  case BLE_READ_MAC: // 读取蓝牙MAC地址的响应 0xC001
    event.type = BLE_EVENT_GET_BLE_MAC_ADDRESS_EVENT;
    event.data_mark = data_mark;
    event.data.get_ble_mac_address_response.mac_address[0] = payload[0];
    event.data.get_ble_mac_address_response.mac_address[1] = payload[1];
    event.data.get_ble_mac_address_response.mac_address[2] = payload[2];
    event.data.get_ble_mac_address_response.mac_address[3] = payload[3];
    event.data.get_ble_mac_address_response.mac_address[4] = payload[4];
    event.data.get_ble_mac_address_response.mac_address[5] = payload[5];
    log_i("读取蓝牙MAC地址响应: MAC=%02X:%02X:%02X:%02X:%02X:%02X",
          event.data.get_ble_mac_address_response.mac_address[0],
          event.data.get_ble_mac_address_response.mac_address[1],
          event.data.get_ble_mac_address_response.mac_address[2],
          event.data.get_ble_mac_address_response.mac_address[3],
          event.data.get_ble_mac_address_response.mac_address[4],
          event.data.get_ble_mac_address_response.mac_address[5]);
    // 触发回调
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;
  case BLE_READ_FIRMWARE_VERSION: // 读取固件版本的响应 0xB011
    event.type = BLE_EVENT_GET_FIRMWARE_VERSION_EVENT;
    event.data_mark = data_mark;
    event.data.get_firmware_version_response.major = payload[0];
    event.data.get_firmware_version_response.minor = payload[1];
    event.data.get_firmware_version_response.patch = payload[2];
    log_i("读取固件版本响应: 版本=%d.%d.%d",
          event.data.get_firmware_version_response.major,
          event.data.get_firmware_version_response.minor,
          event.data.get_firmware_version_response.patch);
    // 触发回调
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;

  default:
    log_d("未处理的读响应: 0x%04X", data_mark);
    break;
  }
}
/**
 * @brief
 * 处理写响应,处理0x93的写响应，其中有个例外，有个写响应是老的协议，设置广播数据块，响应是0x91，我们不管
 * @note
 * 设置的响应协议响应支持：设置广播参数的响应，设置发射功率的响应，设置动态更新广播数据的响应，设置蓝牙系统状态的响应
 */
static void handle_write_response(const uint8_t *frame, uint16_t data_mark) {
  const uint8_t *payload = &frame[INDEX_VOLUME_DATA];
  BleProtocolEvent event = {0};

  log_d("处理写响应: 数据标识=0x%04X", data_mark);

  switch (data_mark) {
  case BLE_SET_BROADCAST_PARAMS: // 设置广播参数的响应 0xB012
    event.type = BLE_EVENT_SET_BROADCAST_PARAMS_EVENT;
    event.data_mark = data_mark;
    // 解析数据域信号强度值
    event.data.set_broadcast_params_response.broadcasting_mode = payload[0];
    event.data.set_broadcast_params_response.broadcasting_time_interval[0] =
        payload[1];
    event.data.set_broadcast_params_response.broadcasting_time_interval[1] =
        payload[2];
    log_i(
        "设置广播参数响应: 模式=%d, 时间间隔=%dms",
        event.data.set_broadcast_params_response.broadcasting_mode,
        (event.data.set_broadcast_params_response.broadcasting_time_interval[1]
         << 8) |
            event.data.set_broadcast_params_response
                .broadcasting_time_interval[0]);
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;
  case BLE_SET_BROADCAST_EMMIT_POWER: // 设置发射功率的响应 0xB013
    event.type = BLE_EVENT_SET_EMMIT_POWER_EVENT;
    event.data_mark = data_mark;
    event.data.set_emit_power_response.emit_power = payload[0];
    log_i("设置发射功率响应: 功率=%02x",
          event.data.set_emit_power_response.emit_power);
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;

  case BLE_SET_BROADCAST_UPDATE_DATA: // 设置广播数据的响应 0xB014
    event.type = BLE_EVENT_DYAMIC_UPDATE_BROADCAST_DATA_EVENT;
    event.data_mark = data_mark;
    event.data.dynamic_update_broadcast_data_response.update_status =
        payload[0]; //
    log_d("设置动态更新广播数据响应: 更新状态%d=%s",
          event.data.dynamic_update_broadcast_data_response.update_status,
          event.data.dynamic_update_broadcast_data_response.update_status
              ? "失败"
              : "成功");
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;
  case BLE_SET_BLUETOOTH_SYSTEM_STATUS: // 设置蓝牙系统状态的响应 0xB000
    event.type = BLE_EVENT_SET_BLUETOOTH_SYSTEM_STATUS_EVENT;
    event.data_mark = data_mark;
    event.data.set_system_status_response.bluetooth_system_status = payload[0];
    log_d("设置蓝牙系统状态响应: 系统状态=%d=%s",
          event.data.set_system_status_response.bluetooth_system_status,
          event.data.set_system_status_response.bluetooth_system_status
              ? "设置失败"
              : "设置成功");
    if (s_ble_event_callback) {
      s_ble_event_callback(&event);
    }
    break;
  default:
    log_d("未处理的写响应: 0x%04X", data_mark);
    break;
  }
}

/*============ 命令发送实现 ============*/

/**
 * @brief 构建DS809蓝牙模组命令帧
 *
 * DS809帧格式: FC 0xAA FC ctrl len mark_lo mark_hi [payload] checksum FB
 *   - FC(1) + AA(1) + FC(1) + ctrl(1) + len(1) + data_field(len bytes) +
 * checksum(1) + FB(1)
 *   - len = sizeof(data_mark) + payload_len = 2 + payload_len
 *
 * @param buf       输出缓冲区 (至少 DGM_TX_BUF_SIZE 字节)
 * @param ctrl_code 控制码  (MCU_READ_CODE / MCU_SET_CODE / MCU_RESPONSE_CODE)
 * @param data_mark 数据标识 (LE16, 如 0xB00E)
 * @param data      数据域载荷 (可为NULL)
 * @param data_len  载荷长度
 * @return 帧总长度
 */
static uint16_t build_cmd_frame(uint8_t *buf, uint8_t ctrl_code,
                                uint16_t data_mark, const uint8_t *data,
                                uint16_t data_len) {
  if (buf == NULL) {
    log_e("build_cmd_frame: buf为空");
    return 0;
  }

  uint16_t pos = 0;

  buf[pos++] = FRAME_HEAD_BLE; // FC
  buf[pos++] = 0xAA;           // 地址
  buf[pos++] = FRAME_HEAD_BLE; // FC
  buf[pos++] = ctrl_code;

  // 数据域长度: 数据标识(2字节) + 载荷
  uint8_t field_len = (uint8_t)(2 + data_len);
  buf[pos++] = field_len;

  // 数据标识 (小端)
  WRITE_LE_U16(&buf[pos], data_mark);
  pos += 2;

  // 载荷
  if (data != NULL && data_len > 0) {
    memcpy(&buf[pos], data, data_len);
    pos += data_len;
  }

  // 校验和 (FC..最后载荷字节 的累加和低8位)
  buf[pos] = util_checksum_sum8(buf, pos);
  pos++;

  buf[pos++] = FRAME_TAIL_BLE; // FB
  return pos;
}

/**
 * @brief 发送读命令 (MCU → 模组, ctrl=0x12)
 */
static bool send_read_cmd(uint16_t data_mark) {
  if (s_send_func == NULL) {
    log_e("发送函数未设置");
    return false;
  }

  uint16_t frame_len =
      build_cmd_frame(s_tx_buffer, MCU_READ_CODE, data_mark, NULL, 0);

  log_d("发送读命令: 数据标识=0x%04X, 长度=%d", data_mark, frame_len);
  elog_hexdump("DGM_TX", 8, s_tx_buffer, frame_len);

  s_send_func(s_tx_buffer, frame_len);
  return true;
}

/**
 * @brief 发送写命令 (MCU → 模组, ctrl=0x13)
 */
static bool send_write_cmd(uint16_t data_mark, const uint8_t *data,
                           uint16_t data_len) {
  if (s_send_func == NULL) {
    log_e("发送函数未设置");
    return false;
  }

  uint16_t frame_len =
      build_cmd_frame(s_tx_buffer, MCU_SET_CODE, data_mark, data, data_len);

  log_d("发送写命令: 数据标识=0x%04X, 长度=%d", data_mark, frame_len);
  elog_hexdump("DGM_TX", 8, s_tx_buffer, frame_len);

  s_send_func(s_tx_buffer, frame_len);
  return true;
}

/**
 * @brief 发送响应帧给模组 (MCU → 模组, ctrl=0x91, 回复模组的0x11请求)
 */
static bool send_response_cmd(uint16_t data_mark, const uint8_t *data,
                              uint16_t data_len) {
  if (s_send_func == NULL) {
    log_e("发送函数未设置");
    return false;
  }

  uint16_t frame_len = build_cmd_frame(s_tx_buffer, MCU_RESPONSE_CODE,
                                       data_mark, data, data_len);

  log_d("发送响应帧: 数据标识=0x%04X, 长度=%d", data_mark, frame_len);
  elog_hexdump("DGM_TX", 8, s_tx_buffer, frame_len);

  s_send_func(s_tx_buffer, frame_len);
  return true;
}

/*============ 公共API — DS809 蓝牙模组控制 ============*/

/**
 * @brief 读取蓝牙模组当前连接状态 (0xB00E)
 */
void BLE_DS809_ReadConnectionStatus(void) {
  send_read_cmd(BLE_READ_CURRENT_CONNECT_STATUS);
}

/**
 * @brief 读取蓝牙模组固件版本 (0xB011)
 */
void BLE_DS809_ReadFirmwareVersion(void) {
  send_read_cmd(BLE_READ_FIRMWARE_VERSION);
}

/**
 * @brief 读取蓝牙MAC地址 (0xC001)
 */
void BLE_DS809_ReadMacAddress(void) { send_read_cmd(BLE_READ_MAC); }

/**
 * @brief 设置广播参数 (0xB012)
 * @param mode         广播模式: 0x00=停止, 0x01=慢速(2s), 0x02=快速(45ms)
 * @param interval_ms  广播间隔(ms, 20~5000), mode=0x02时有效
 */
void BLE_DS809_SetBroadcastParams(uint8_t mode, uint16_t interval_ms) {
  uint8_t data[3];
  data[0] = mode;
  data[1] = (uint8_t)(interval_ms & 0xFF);
  data[2] = (uint8_t)(interval_ms >> 8);
  send_write_cmd(BLE_SET_BROADCAST_PARAMS, data, sizeof(data));
}

/**
 * @brief 设置发射功率 (0xB013)
 * @param power  0x00=最小, 0x01=中等, 0x02=最大(+4dBm)
 */
void BLE_DS809_SetEmitPower(uint8_t power) {
  send_write_cmd(BLE_SET_BROADCAST_EMMIT_POWER, &power, 1);
}

/**
 * @brief 设置蓝牙系统状态 (0xB000)
 * @param status  系统控制字节 (如复位、进入/退出低功耗)
 */
void BLE_DS809_SetSystemStatus(uint8_t status) {
  send_write_cmd(BLE_SET_BLUETOOTH_SYSTEM_STATUS, &status, 1);
}

/**
 * @brief 响应模组的工作参数请求 (0xB007)
 *
 * 蓝牙模组上电后主动发起 0x11/0xB007 请求，MCU必须回复此帧，
 * 告知模组 MAC 地址、配对密码和广播设备名称。
 *
 * @param config  包含 mac_address(6), pairing_password(4),
 *                device_name(8), broadcast_status(1) 的配置结构体
 */
void BLE_DS809_ReplyWorkParam(
    const BleQueryConfigureBluetoothResponse *config) {
  if (config == NULL) {
    log_e("BLE_DS809_ReplyWorkParam: config为NULL");
    return;
  }

  uint8_t payload[19];
  memcpy(&payload[0], config->mac_address, 6);
  memcpy(&payload[6], config->pairing_password, 4);
  memcpy(&payload[10], config->device_name, 8);
  payload[18] = config->broadcast_status;

  log_i("回复工作参数: MAC=%02X:%02X:%02X:%02X:%02X:%02X, 广播=%d", payload[0],
        payload[1], payload[2], payload[3], payload[4], payload[5],
        payload[18]);

  send_response_cmd(BLE_CONFIG_WORK_PARAM, payload, sizeof(payload));
}

/**
 * @brief 响应模组的UUID参数请求 (0xB00D)
 *
 * 蓝牙模组上电后可能主动发起 0x11/0xB00D 请求，MCU如不需自定义UUID
 * 可回复空数据域，模组将使用默认UUID。
 *
 * @param uuid_data  UUID配置数据 (NULL则回复空数据域使用默认UUID)
 * @param len        数据长度 (0则使用默认UUID)
 */
void BLE_DS809_ReplyUUIDParam(const uint8_t *uuid_data, uint16_t len) {
  if (uuid_data == NULL || len == 0) {
    // 回复空数据域，使用模组默认UUID
    send_response_cmd(BLE_CONFIG_UUID_PARAM, NULL, 0);
    log_i("回复UUID参数: 使用默认UUID");
  } else {
    send_response_cmd(BLE_CONFIG_UUID_PARAM, uuid_data, len);
    log_i("回复UUID参数: 自定义UUID, 长度=%d", len);
  }
}

/**
 * @brief 设置蓝牙协议事件回调
 * @param callback 回调函数
 */
void DGM_SetEventCallback(BleEventCallback callback) {
  s_ble_event_callback = callback;
}

/**
 * @brief 获取事件类型名称 (用于调试日志)
 * @param type 事件类型
 * @return 事件名称字符串
 */
const char *DGM_GetEventName(BLE_EventType type) {
  static const char *event_names[] = {
      [BLE_EVENT_TYPE_NONE_EVENT] = "无事件",
      [BLE_EVENT_SET_BROADCASET_SELF_DEFINE_DATA_EVENT] = "设置广播自定义数据",
      [BLE_EVENT_CONFIGURE_UUID_EVENT] = "配置UUID请求",
      [BLE_EVENT_CONFIGURE_BLUETOOTH_EVENT] = "配置工作参数请求",
      [BLE_EVENT_GET_CURRENT_IF_CONNECTED_EVENT] = "获取连接状态",
      [BLE_EVENT_GET_BLE_MAC_ADDRESS_EVENT] = "获取蓝牙MAC地址",
      [BLE_EVENT_DYAMIC_UPDATE_SCAN_RESPONSE_DATA_EVENT] =
          "动态更新扫描响应数据",
      [BLE_EVENT_GET_FIRMWARE_VERSION_EVENT] = "获取固件版本",
      [BLE_EVENT_SET_BROADCAST_PARAMS_EVENT] = "设置广播参数",
      [BLE_EVENT_SET_EMMIT_POWER_EVENT] = "设置发射功率",
      [BLE_EVENT_DYAMIC_UPDATE_BROADCAST_DATA_EVENT] = "动态更新广播数据",
      [BLE_EVENT_SET_BLUETOOTH_SYSTEM_STATUS_EVENT] = "设置系统状态",
      [BLE_EVENT_PARSE_ERROR_EVENT] = "解析错误",
      [BLE_EVENT_CHECKSUM_ERROR_EVENT] = "校验和错误",
      [BLE_EVENT_TIMEOUT_EVENT] = "超时",
  };

  if ((uint8_t)type < (uint8_t)BLE_EVENT_MAX) {
    return event_names[type];
  }
  return "未知事件";
}
