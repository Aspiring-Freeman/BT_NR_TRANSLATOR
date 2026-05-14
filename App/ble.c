/**
 * @file    ble.c
 * @brief   蓝牙应用层 — 威星 1P / DS809 模组初始化与事件处理
 *
 * 数据流:
 *
 *    DS809模组 ─UART4─► BLEUart_Rx_Process ─► ProtocolManager_Device_Parse
 *                                                  │
 *                                                  ▼
 *                                          ble_parse() (bt_ds809.c)
 *                                                  │
 *                                          BleProtocolEvent
 *                                                  ▼
 *                                  ble_on_protocol_event() (本文件)
 *                                                  │
 *                                                  ▼
 *                            · 自动响应 B007/B00D
 *                            · 更新连接/MAC/版本缓存
 *                            · 通知业务层
 */
#include "ble.h"
#include "Peripheral/uart/ble_uart.h"
#include "Protocol/App/BT_Passthrough/bt_app_proto.h"
#include "Protocol/App/BT_Passthrough/bt_ds809.h"
#include "TimeManager/time_manager.h"
#include "app_config.h"
#include "protocol_manager.h"
#include <elog.h>
#include <string.h>

#undef LOG_TAG
#define LOG_TAG "ble_app"

/* 上电握手最长等待时间 (ms)。模组实际 5 次重试 ≈ 500ms，留 3s 余量给上电延时 */
#define BLE_HANDSHAKE_TIMEOUT_MS 3000U
/* 主动状态查询节流间隔 (ms) */
#define BLE_STATUS_QUERY_PERIOD_MS 5000U

/*============================================================================
 *  内部状态
 *===========================================================================*/

typedef enum {
  BLE_BOOT_IDLE = 0,       /* 未初始化 */
  BLE_BOOT_WAIT_HANDSHAKE, /* 已初始化，等待模组发起 B007 */
  BLE_BOOT_HANDSHAKE_DONE, /* B007 已响应，准备就绪 */
  BLE_BOOT_TIMEOUT,        /* 超时未收到 B007 (UART 故障?) */
} BleBootState;

static BleBootState s_boot_state = BLE_BOOT_IDLE;
static uint32_t s_boot_start_tick = 0;
static uint32_t s_last_query_tick = 0;
static bool s_warned_timeout = false;

static bool s_connected = false;

static struct {
  bool valid;
  uint8_t addr[6];
} s_mac_cached;

static struct {
  bool valid;
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
} s_fw_ver;

/* 用户提供的 B007 响应配置 — 默认值见 BLE_INIT() */
static BleQueryConfigureBluetoothResponse s_work_param;

static BleQueryConfigureUUIDResponse s_uuid_param;

/*============================================================================
 *  事件回调 — 协议层 → 应用层
 *===========================================================================*/

static void ble_on_protocol_event(const BleProtocolEvent *event) {
  if (event == NULL) {
    return;
  }

  log_d("BLE事件: %s (mark=0x%04X)", DGM_GetEventName(event->type),
        event->data_mark);

  switch (event->type) {

  /* ─── 模组上电主动请求：必须立即回复，否则模组用默认 MAC/UUID ─── */
  case BLE_EVENT_CONFIGURE_BLUETOOTH_EVENT: /* B007 */
    log_i("[握手] 收到 B007 工作参数请求, 回复中...");
    BLE_DS809_ReplyWorkParam(&s_work_param);
    if (s_boot_state == BLE_BOOT_WAIT_HANDSHAKE) {
      s_boot_state = BLE_BOOT_HANDSHAKE_DONE;
      log_i("[握手] B007 完成");
    }
    break;

  case BLE_EVENT_CONFIGURE_UUID_EVENT: /* B00D */
    log_i("[握手] 收到 B00D UUID请求, 配置uuid");
    BLE_DS809_ReplyUUIDParam((const uint8_t *)&s_uuid_param,
                             sizeof(s_uuid_param));
    break;

  /* ─── 读响应：缓存供上层查询 ─── */
  case BLE_EVENT_GET_CURRENT_IF_CONNECTED_EVENT: /* B00E */ {
    bool now = (event->data.get_connection_status_response.connected != 0);
    if (now != s_connected) {
      log_i("BLE 连接状态: %s → %s", s_connected ? "连接" : "断开",
            now ? "连接" : "断开");
    }
    s_connected = now;
    break;
  }

  case BLE_EVENT_GET_BLE_MAC_ADDRESS_EVENT: /* C001 */
    memcpy(s_mac_cached.addr,
           event->data.get_ble_mac_address_response.mac_address, 6);
    s_mac_cached.valid = true;
    log_i("BLE 实际 MAC: %02X:%02X:%02X:%02X:%02X:%02X", s_mac_cached.addr[0],
          s_mac_cached.addr[1], s_mac_cached.addr[2], s_mac_cached.addr[3],
          s_mac_cached.addr[4], s_mac_cached.addr[5]);
    break;

  case BLE_EVENT_GET_FIRMWARE_VERSION_EVENT: /* B011 */
    s_fw_ver.major = event->data.get_firmware_version_response.major;
    s_fw_ver.minor = event->data.get_firmware_version_response.minor;
    s_fw_ver.patch = event->data.get_firmware_version_response.patch;
    s_fw_ver.valid = true;
    log_i("BLE 模组固件: V%d.%d.%d", s_fw_ver.major, s_fw_ver.minor,
          s_fw_ver.patch);
    break;

  /* ─── 设置应答：仅记录日志 ─── */
  case BLE_EVENT_SET_BROADCAST_PARAMS_EVENT:
  case BLE_EVENT_SET_EMMIT_POWER_EVENT:
  case BLE_EVENT_SET_BLUETOOTH_SYSTEM_STATUS_EVENT:
  case BLE_EVENT_DYAMIC_UPDATE_BROADCAST_DATA_EVENT:
  case BLE_EVENT_DYAMIC_UPDATE_SCAN_RESPONSE_DATA_EVENT:
  case BLE_EVENT_SET_BROADCASET_SELF_DEFINE_DATA_EVENT:
    log_d("BLE 设置应答: %s", DGM_GetEventName(event->type));
    break;

  /* ─── 错误事件 ─── */
  case BLE_EVENT_PARSE_ERROR_EVENT:
  case BLE_EVENT_CHECKSUM_ERROR_EVENT:
    log_e("BLE 协议错误: %s", DGM_GetEventName(event->type));
    break;

  case BLE_EVENT_TIMEOUT_EVENT:
    log_w("BLE 超时事件");
    break;

  default:
    log_d("BLE 未处理事件: %d", event->type);
    break;
  }
}

/*============================================================================
 *  bt_app 应用层事件回调 (BB01/BB02/AAFF)
 *  — 默认仅记录日志并回一个空应答，具体业务逻辑由上层填充。
 *===========================================================================*/
static uint8_t s_report_name[21] = "BT-FT-DEFAULT-000000"; /* 21B ASCII */

static void ble_on_app_event(const BtAppEvent *ev) {
  if (ev == NULL)
    return;

  log_i("[BT_APP] %s did=0x%04X ctrl=0x%02X len=%u",
        BT_APP_GetEventName(ev->type), ev->data_id, ev->ctrl,
        (unsigned)ev->payload_len);

  switch (ev->type) {

  /* ─── 0xBB01 触发通讯启动及测试 ─── */
  case BT_APP_EVENT_TRIG_TEST_DOWN:
    /* APP 下发触发测试请求 → 回 21B 报警器编号 */
    BT_APP_SendTrigTestUp(ev->target_addr, ev->bcd_time, s_report_name);
    break;

  case BT_APP_EVENT_TRIG_TEST_UP:
    log_i("[BB01] 报警器编号=%.21s", ev->data.trig_test_up.report_name);
    break;

  /* ─── 0xBB02 设置/读取配对信息 ─── */
  case BT_APP_EVENT_PAIR_SET_DOWN:
    /* TODO: 业务侧入库/刷新本地配对表; 这里先原样过手回应 */
    log_i("[BB02-写下行] 待配对 %u 台, 默认返回成功",
          ev->data.pair_set_down.dev_count);
    BT_APP_SendPairSetResultUp(ev->target_addr, ev->bcd_time, /*result=*/0,
                               ev->data.pair_set_down.dev_list,
                               ev->data.pair_set_down.dev_count);
    break;

  case BT_APP_EVENT_PAIR_READ_DOWN:
    /* TODO: 业务侧查本地配对表后填充。这里先返回空列表 */
    BT_APP_SendPairReadResultUp(ev->target_addr, ev->bcd_time, NULL, 0);
    break;

  case BT_APP_EVENT_PAIR_SET_UP:
  case BT_APP_EVENT_PAIR_READ_UP:
    /* 上行帧通常在本设备作为主动者时会收到 (微型机会收到)，这里仅日志 */
    break;

  /* ─── 0xAAFF 设备列表同步 ─── */
  case BT_APP_EVENT_DEV_LIST_SYNC_DOWN:
    /* TODO: 启动 BLE 扫描, 扫描结果可调 BT_APP_SendDevListSyncUp */
    BT_APP_SendDevListSyncUp(ev->target_addr, ev->bcd_time, NULL, 0);
    break;

  case BT_APP_EVENT_DEV_LIST_SYNC_UP:
    log_i("[AAFF] 收到扫描结果, len=%u", ev->data.dev_list.scan_len);
    break;

  case BT_APP_EVENT_UNKNOWN_DID:
  case BT_APP_EVENT_PARSE_ERROR:
    log_w("[BT_APP] 异常: %s", BT_APP_GetEventName(ev->type));
    break;

  default:
    break;
  }
}

/*============================================================================
 *  配置接口 (在 BLE_INIT 之前调用)
 *===========================================================================*/

void BLE_SetMacAddress(const uint8_t mac[6]) {
  if (mac == NULL)
    return;
  memcpy(s_work_param.mac_address, mac, 6);
}

void BLE_SetDeviceName(const char *name, uint8_t len) {
  if (name == NULL)
    return;
  if (len > sizeof(s_work_param.device_name)) {
    len = sizeof(s_work_param.device_name);
  }
  memset(s_work_param.device_name, 0, sizeof(s_work_param.device_name));
  memcpy(s_work_param.device_name, name, len);
}

void BLE_SetPairingPassword(const uint8_t password[4]) {
  if (password == NULL)
    return;
  memcpy(s_work_param.pairing_password, password, 4);
}

/*============================================================================
 *  初始化与周期任务
 *===========================================================================*/

void BLE_INIT(void) {
  /* ─── ① 默认 B007 响应 (用户可在 BLE_INIT 之前用 setter 覆盖) ─── */
  /* 默认 MAC = 全 0；模组会回退到芯片烧录的随机静态地址 */
  static const BleQueryConfigureBluetoothResponse default_param = {
      .mac_address = {0xC1, 0x00, 0x00, 0x00, 0x00, 0x01},
      .pairing_password = {0xFF, 0xFF, 0xFF, 0xFF}, /* 不设密码 */
      .device_name = "BT-FT\0\0\0",                 /* 8 字节, 末尾补 0 */
      .broadcast_status = 0x01,                     /* 启动后立即广播 */
  };
  static const BleQueryConfigureUUIDResponse default_uuid_param = {
      .uuid_attribute = 0x00,  /* 固定为0,蓝牙服务按照 Write/Notify 排序； */
      .service_uuid[0] = 0xF2, /* Device Information Service */
      .service_uuid[1] = 0xE0,
      .characteristic_uuid1[0] = 0xF2, /* write  */
      .characteristic_uuid1[1] = 0xE1,
      .characteristic_uuid2[0] = 0xF2, /* notify */
      .characteristic_uuid2[1] = 0xE2,
  };
  /* 仅在用户从未配置过时填默认值; 用 device_name 是否仍为 0 判断 */
  if (s_work_param.device_name[0] == 0 && s_work_param.device_name[1] == 0) {
    s_work_param = default_param;
    s_uuid_param = default_uuid_param;
  }

  /* ─── ② 注册协议到 ProtocolManager 设备槽 ─── */
  ProtocolManager_RegisterDevice(&diaphragm_gas_meter_protocol);
  ProtocolManager_SetActiveDevice("diaphragm_gas_meter");

  /* ─── ②.b 注册并初始化 bt_app 应用层协议 (与 DS809 并行存在)
   *  ProtocolManager 设备槽只能有一个“活跃”协议 (当前为 DS809),
   *  因此 bt_app 不走 ProtocolManager_Device_Parse, 而是由 BLE_OnRxData
   *  直接调 bt_app_protocol.parse() 并行喂字节 */
  if (bt_app_protocol.init) {
    bt_app_protocol.init();
  }
  if (bt_app_protocol.set_send_func) {
    bt_app_protocol.set_send_func((ProtocolSendFunc)BLEUart_Tx_Send);
  }
  BT_APP_SetEventCallback(ble_on_app_event);

  /* ─── ③ 绑定 UART4 发送函数 ─── */
  ProtocolManager_SetDeviceSendFunc((ProtocolSendFunc)BLEUart_Tx_Send);

  /* ─── ④ 注册事件回调 (B007 自动应答的关键) ─── */
  DGM_SetEventCallback(ble_on_protocol_event);

  /* ─── ⑤ 启动握手监控 ─── */
  s_boot_state = BLE_BOOT_WAIT_HANDSHAKE;
  s_boot_start_tick = TM_GetTick();
  s_last_query_tick = 0;
  s_warned_timeout = false;

  log_i("[BLE] 应用层初始化完成, 等待模组上电握手...");
  log_i("[BLE] 设备名='%s' 广播状态=%d", s_work_param.device_name,
        s_work_param.broadcast_status);
}

void BLE_Task(void) {
  uint32_t now = TM_GetTick();

  switch (s_boot_state) {

  case BLE_BOOT_IDLE:
    /* 未初始化, 不做任何事 */
    return;

  case BLE_BOOT_WAIT_HANDSHAKE:
    /* 监控握手超时 — 通常是 UART 接反或波特率不对 */
    if ((now - s_boot_start_tick) > BLE_HANDSHAKE_TIMEOUT_MS) {
      if (!s_warned_timeout) {
        log_e("[BLE] 握手超时! %ums 内未收到模组 B007 请求",
              (unsigned)(now - s_boot_start_tick));
        log_e("[BLE] 请检查: ① UART4 RX/TX 接线 ② 波特率 9600 8N1 ③ 模组供电");
        s_warned_timeout = true;
      }
      s_boot_state = BLE_BOOT_TIMEOUT;
    }
    break;

  case BLE_BOOT_HANDSHAKE_DONE:
    /* 握手成功后, 异步查询模组的固件版本和实际 MAC 一次, 缓存供上层使用 */
    if (!s_fw_ver.valid && (now - s_last_query_tick) > 200U) {
      BLE_DS809_ReadFirmwareVersion();
      s_last_query_tick = now;
    } else if (s_fw_ver.valid && !s_mac_cached.valid &&
               (now - s_last_query_tick) > 200U) {
      BLE_DS809_ReadMacAddress();
      s_last_query_tick = now;
    } else if (s_fw_ver.valid && s_mac_cached.valid &&
               (now - s_last_query_tick) > BLE_STATUS_QUERY_PERIOD_MS) {
      /* 周期性刷新连接状态 (也可改用 P0.5 硬件信号触发) */
      BLE_DS809_ReadConnectionStatus();
      s_last_query_tick = now;
    }
    break;

  case BLE_BOOT_TIMEOUT:
    /* 模组可能稍后才上电, 继续容忍后续的 B007 请求；事件处理里会切回 DONE */
    break;
  }
}

/*============================================================================
 *  状态查询接口
 *===========================================================================*/

bool BLE_IsConnected(void) { return s_connected; }

bool BLE_IsReady(void) { return s_boot_state == BLE_BOOT_HANDSHAKE_DONE; }

bool BLE_GetMacAddress(uint8_t mac_out[6]) {
  if (!s_mac_cached.valid || mac_out == NULL) {
    return false;
  }
  memcpy(mac_out, s_mac_cached.addr, 6);
  return true;
}

bool BLE_GetFirmwareVersion(uint8_t *major, uint8_t *minor, uint8_t *patch) {
  if (!s_fw_ver.valid) {
    return false;
  }
  if (major)
    *major = s_fw_ver.major;
  if (minor)
    *minor = s_fw_ver.minor;
  if (patch)
    *patch = s_fw_ver.patch;
  return true;
}

/*============================================================================
 *  BLE RX 分发 (DS809 握手 + bt_app 应用层 双解析)
 *===========================================================================*/
void BLE_OnRxData(uint8_t *buf, uint16_t len) {
  if (buf == NULL || len == 0) {
    return;
  }
  /* ① DS809 透传协议 (B007/B00D 握手、B00E/C001 查询应答等) */
  ProtocolManager_Device_Parse(buf, len);

  /* ② 应用层 BB01/BB02/AAFF — 与 DS809 共用同一路 UART。
   *    两个解析器各自识别帧头 (DS809 以 0xA5/0xA8 起头, bt_app 以 0xFE 0xFE
   * 0xFE 0x68 起头), 互不干扰。 */
  if (bt_app_protocol.parse) {
    bt_app_protocol.parse(buf, len);
  }
}
