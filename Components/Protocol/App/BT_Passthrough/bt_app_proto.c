#include "bt_app_proto.h"
#include <string.h>

#define LOG_TAG "bt_app_proto"
#include <elog.h>

/* 状态机 */
enum {
  S_WAIT_START1 = 0, /* 跳过任意 0xFE，等待 0x68 */
  S_TARGET_ADDR,
  S_START2,
  S_CTRL,
  S_DATA_LEN_LO,
  S_DATA_LEN_HI,
  S_DATA,
  S_CHECKSUM,
  S_END,
};

static inline void parser_to_idle(bt_app_parser_t *p) {
  p->state = S_WAIT_START1;
  p->addr_cnt = 0;
  p->data_cnt = 0;
  p->data_len = 0;
  p->sum = 0;
  p->buf_idx = 0;
}

void bt_app_parser_init(bt_app_parser_t *p, bt_app_frame_cb_t cb, void *user) {
  if (p == NULL)
    return;
  p->cb = cb;
  p->cb_user = user;
  parser_to_idle(p);
}

void bt_app_parser_reset(bt_app_parser_t *p) {
  if (p == NULL)
    return;
  parser_to_idle(p);
}

/** 缓冲并累加；超限则复位返回 false */
static inline bool buf_push(bt_app_parser_t *p, uint8_t b) {
  if (p->buf_idx >= sizeof(p->buf)) {
    parser_to_idle(p);
    return false;
  }
  p->buf[p->buf_idx++] = b;
  p->sum = (uint8_t)(p->sum + b);
  return true;
}

/** 整帧到达 → 回调 */
static void deliver(bt_app_parser_t *p) {
  if (p->cb == NULL)
    return;

  /* buf 布局:
   * [0]    start1 = 0x68
   * [1..6] target_addr
   * [7]    start2 = 0x68
   * [8]    ctrl
   * [9..10] data_len (LE)
   * [11..11+data_len-1] 数据域
   */
  bt_app_frame_t f;
  (void)memcpy(f.target_addr, &p->buf[1], BT_APP_TARGET_ADDR_LEN);
  f.ctrl = p->buf[8];
  f.data_len = p->data_len;

  const uint8_t *d = &p->buf[11];
  (void)memcpy(f.bcd_time, d, 6);
  f.dev_type = d[6];
  f.data_id = (uint16_t)(((uint16_t)d[7] << 8) | d[8]); /* MSB first */
  f.frame_seq = d[9];
  f.payload_len = (uint16_t)(p->data_len - BT_APP_DATA_HEAD_LEN);
  f.payload = (f.payload_len > 0) ? &d[10] : NULL;

  p->cb(&f, p->cb_user);
}

void bt_app_parser_feed(bt_app_parser_t *p, const uint8_t *data, size_t len) {
  if (p == NULL || data == NULL)
    return;

  for (size_t i = 0; i < len; ++i) {
    const uint8_t b = data[i];

    switch (p->state) {

    case S_WAIT_START1:
      /* 容忍任意数量前导码 0xFE，遇 0x68 即开始 */
      if (b == BT_APP_START_BYTE) {
        p->buf_idx = 0;
        p->sum = 0;
        (void)buf_push(p, b);
        p->addr_cnt = 0;
        p->state = S_TARGET_ADDR;
      }
      /* 0xFE 与其他噪声字节均丢弃 */
      break;

    case S_TARGET_ADDR:
      if (!buf_push(p, b))
        break;
      if (++p->addr_cnt >= BT_APP_TARGET_ADDR_LEN) {
        p->state = S_START2;
      }
      break;

    case S_START2:
      if (b != BT_APP_START_BYTE) {
        /* 同步失败：若当前是 0x68，复用为新帧 start1 */
        parser_to_idle(p);
        if (b == BT_APP_START_BYTE) {
          (void)buf_push(p, b);
          p->state = S_TARGET_ADDR;
        }
        break;
      }
      (void)buf_push(p, b);
      p->state = S_CTRL;
      break;

    case S_CTRL:
      if (!buf_push(p, b))
        break;
      p->state = S_DATA_LEN_LO;
      break;

    case S_DATA_LEN_LO:
      if (!buf_push(p, b))
        break;
      p->data_len = b;
      p->state = S_DATA_LEN_HI;
      break;

    case S_DATA_LEN_HI:
      if (!buf_push(p, b))
        break;
      p->data_len |= (uint16_t)b << 8;
      if (p->data_len < BT_APP_DATA_HEAD_LEN ||
          p->data_len > BT_APP_MAX_DATA_LEN) {
        parser_to_idle(p);
        break;
      }
      p->data_cnt = 0;
      p->state = S_DATA;
      break;

    case S_DATA:
      if (!buf_push(p, b))
        break;
      if (++p->data_cnt >= p->data_len) {
        p->state = S_CHECKSUM;
      }
      break;

    case S_CHECKSUM:
      /* sum 此刻 = start1 ~ 数据域末尾 累加和 */
      if (b != p->sum) {
        parser_to_idle(p);
        break;
      }
      p->state = S_END;
      break;

    case S_END:
      if (b == BT_APP_END_BYTE) {
        deliver(p);
      }
      parser_to_idle(p);
      break;

    default:
      parser_to_idle(p);
      break;
    }
  }
}

size_t bt_app_build_frame(uint8_t *buf, size_t buf_size,
                          const uint8_t target_addr[BT_APP_TARGET_ADDR_LEN],
                          uint8_t ctrl, const uint8_t bcd_time[6],
                          uint8_t dev_type, uint16_t data_id, uint8_t frame_seq,
                          const uint8_t *payload, uint16_t payload_len) {
  if (buf == NULL)
    return 0;
  if (payload_len > (uint16_t)(BT_APP_MAX_DATA_LEN - BT_APP_DATA_HEAD_LEN))
    return 0;

  const uint16_t data_len = (uint16_t)(BT_APP_DATA_HEAD_LEN + payload_len);
  const size_t need = (size_t)BT_APP_FIXED_OVERHEAD + data_len;
  if (buf_size < need)
    return 0;

  uint8_t *p = buf;

  /* 前导码 */
  for (uint32_t i = 0; i < BT_APP_PREAMBLE_LEN; ++i)
    *p++ = BT_APP_PREAMBLE_BYTE;

  /* 累加和起点 = start1 */
  uint8_t *const sum_start = p;
  *p++ = BT_APP_START_BYTE;

  /* 目标地址 */
  if (target_addr)
    (void)memcpy(p, target_addr, BT_APP_TARGET_ADDR_LEN);
  else
    (void)memset(p, 0xFF, BT_APP_TARGET_ADDR_LEN);
  p += BT_APP_TARGET_ADDR_LEN;

  /* start2 + ctrl */
  *p++ = BT_APP_START_BYTE;
  *p++ = ctrl;

  /* 数据域长度 (低字节在前) */
  *p++ = (uint8_t)(data_len & 0xFFu);
  *p++ = (uint8_t)(data_len >> 8);

  /* 数据域 */
  if (bcd_time)
    (void)memcpy(p, bcd_time, 6);
  else
    (void)memset(p, 0x00, 6);
  p += 6;
  *p++ = dev_type;
  *p++ = (uint8_t)(data_id >> 8); /* MSB first */
  *p++ = (uint8_t)(data_id & 0xFFu);
  *p++ = frame_seq;
  if (payload && payload_len) {
    (void)memcpy(p, payload, payload_len);
    p += payload_len;
  }

  /* 累加和 */
  uint8_t sum = 0;
  for (const uint8_t *q = sum_start; q < p; ++q)
    sum = (uint8_t)(sum + *q);
  *p++ = sum;

  /* 结束符 */
  *p++ = BT_APP_END_BYTE;

  return (size_t)(p - buf);
}

/* ============================================================================
 *                    ProtocolInterface 适配 (供 ProtocolManager 注册)
 * ============================================================================
 *  设计:
 *   - 内部维护单例 parser、单例 send func、单例事件回调
 *   - parse(): 直接喂入 bt_app_parser，整帧到达时由 on_frame() 上抛事件
 *   - send_cmd(cmd, param): cmd = 数据标识 (0xBB01/0xBB02/0xAAFF)；param 为
 *     业务自定义结构体；这里只提供最常用映射，更细的发送由高层 API 调用 build。
 * ==========================================================================*/

#define BT_APP_TX_BUF_SIZE BT_APP_MAX_FRAME_LEN

static ProtocolSendFunc s_send_func = NULL;
static ProtocolEventCallback s_proto_event_cb = NULL; /* 框架级 (调试) */
static BtAppEventCallback s_app_event_cb = NULL;      /* 业务级 */
static bt_app_parser_t s_parser;
static uint8_t s_tx_buffer[BT_APP_TX_BUF_SIZE];

/** 填充事件通用元数据 (从 frame 复制) */
static void fill_event_common(BtAppEvent *ev, const bt_app_frame_t *frame) {
  memcpy(ev->target_addr, frame->target_addr, BT_APP_TARGET_ADDR_LEN);
  ev->ctrl = frame->ctrl;
  ev->data_id = frame->data_id;
  memcpy(ev->bcd_time, frame->bcd_time, 6);
  ev->dev_type = frame->dev_type;
  ev->frame_seq = frame->frame_seq;
  ev->payload = frame->payload;
  ev->payload_len = frame->payload_len;
  ev->frame = frame;
}

static inline void emit_event(const BtAppEvent *ev) {
  log_d("BT_APP 事件: %s did=0x%04X ctrl=0x%02X len=%u",
        BT_APP_GetEventName(ev->type), ev->data_id, ev->ctrl,
        (unsigned)ev->payload_len);
  if (s_app_event_cb) {
    s_app_event_cb(ev);
  }
  if (s_proto_event_cb) {
    s_proto_event_cb(PROTOCOL_EVENT_RECEIVED, ev->data_id, ev->payload,
                     ev->payload_len);
  }
}

/* ----------------------------------------------------------------------------
 *  0xBB01 触发通讯启动及测试
 *  - DOWN (ctrl=0x04): APP 触发，payload 通常为空或携带触发参数
 *  - UP   (ctrl=0x84): 报警器返回 21B ASCII 报警器编号
 * --------------------------------------------------------------------------*/
static void handle_trig_test(const bt_app_frame_t *frame) {
  BtAppEvent ev = {0};
  fill_event_common(&ev, frame);

  if ((frame->ctrl & BT_APP_CTRL_RSP_FLAG) != 0) {
    ev.type = BT_APP_EVENT_TRIG_TEST_UP;
    /* 解析 21B 报警器编号；不足则补 0 */
    memset(ev.data.trig_test_up.report_name, 0,
           sizeof(ev.data.trig_test_up.report_name));
    if (frame->payload && frame->payload_len > 0) {
      uint16_t n = frame->payload_len < 21u ? frame->payload_len : 21u;
      memcpy(ev.data.trig_test_up.report_name, frame->payload, n);
    }
    log_i("[BB01-UP] 报警器编号(21B): %.21s", ev.data.trig_test_up.report_name);
  } else {
    ev.type = BT_APP_EVENT_TRIG_TEST_DOWN;
    log_i("[BB01-DOWN] 触发测试请求, payload_len=%u",
          (unsigned)frame->payload_len);
  }
  emit_event(&ev);
}

/* ----------------------------------------------------------------------------
 *  0xBB02 设置/读取配对信息
 *  - WRITE_REQ  (0x04): payload = N × (1B 类型 + 24B 编号)
 *  - WRITE_RSP  (0x84): payload = 1B result + 1B count + N × (1B + 24B)
 *  - READ_REQ   (0x01): payload 通常为空
 *  - READ_RSP   (0x81): payload = 1B count + N × (1B 类型 + 1B 状态 + 24B + 6B
 * MAC)
 * --------------------------------------------------------------------------*/
static void handle_pair_info(const bt_app_frame_t *frame) {
  BtAppEvent ev = {0};
  fill_event_common(&ev, frame);

  switch (frame->ctrl) {

  case BT_APP_CTRL_WRITE_REQ: {
    ev.type = BT_APP_EVENT_PAIR_SET_DOWN;
    /* payload = (1B + 24B) × N，按 25 整除推断设备数 */
    const uint16_t per = 1u + 24u;
    if (frame->payload && frame->payload_len % per == 0) {
      ev.data.pair_set_down.dev_count = (uint8_t)(frame->payload_len / per);
      ev.data.pair_set_down.dev_list = frame->payload;
    }
    log_i("[BB02-写下行] 待配对 %u 台", ev.data.pair_set_down.dev_count);
    break;
  }

  case BT_APP_CTRL_WRITE_RSP: {
    ev.type = BT_APP_EVENT_PAIR_SET_UP;
    if (frame->payload && frame->payload_len >= 2u) {
      ev.data.pair_set_up.result = frame->payload[0];
      ev.data.pair_set_up.dev_count = frame->payload[1];
      ev.data.pair_set_up.dev_list = &frame->payload[2];
    }
    log_i("[BB02-写上行] result=%u count=%u", ev.data.pair_set_up.result,
          ev.data.pair_set_up.dev_count);
    break;
  }

  case BT_APP_CTRL_READ_REQ:
    ev.type = BT_APP_EVENT_PAIR_READ_DOWN;
    log_i("[BB02-读下行] 请求配对列表");
    break;

  case BT_APP_CTRL_READ_RSP: {
    ev.type = BT_APP_EVENT_PAIR_READ_UP;
    if (frame->payload && frame->payload_len >= 1u) {
      ev.data.pair_read_up.dev_count = frame->payload[0];
      ev.data.pair_read_up.dev_list = &frame->payload[1];
    }
    log_i("[BB02-读上行] count=%u", ev.data.pair_read_up.dev_count);
    break;
  }

  default:
    ev.type = BT_APP_EVENT_UNKNOWN_DID;
    log_w("[BB02] 未知 ctrl=0x%02X", frame->ctrl);
    break;
  }
  emit_event(&ev);
}

/* ----------------------------------------------------------------------------
 *  0xAAFF 设备列表同步
 *  - DOWN (0x01): APP→工装 请求扫描结果
 *  - UP   (0x81): 工装→APP 上传扫描结果
 *  payload = (RSSI1 + MAC6 + 扫描数据n) × N，长度可变，业务层自行二次解析。
 * --------------------------------------------------------------------------*/
static void handle_dev_list_sync(const bt_app_frame_t *frame) {
  BtAppEvent ev = {0};
  fill_event_common(&ev, frame);

  ev.type = ((frame->ctrl & BT_APP_CTRL_RSP_FLAG) != 0)
                ? BT_APP_EVENT_DEV_LIST_SYNC_UP
                : BT_APP_EVENT_DEV_LIST_SYNC_DOWN;

  ev.data.dev_list.scan_blob = frame->payload;
  ev.data.dev_list.scan_len = frame->payload_len;

  log_i("[AAFF-%s] scan_len=%u",
        ev.type == BT_APP_EVENT_DEV_LIST_SYNC_UP ? "上行" : "下行",
        (unsigned)frame->payload_len);
  emit_event(&ev);
}

/** 整帧到达 → 按 data_id 分发到对应 handler */
static void on_frame_received(const bt_app_frame_t *frame, void *user) {
  (void)user;
  if (frame == NULL) {
    return;
  }

  switch (frame->data_id) {
  case BT_APP_DID_TRIG_TEST:
    handle_trig_test(frame);
    break;
  case BT_APP_DID_PAIR_INFO:
    handle_pair_info(frame);
    break;
  case BT_APP_DID_DEV_LIST_SYNC:
    handle_dev_list_sync(frame);
    break;
  default: {
    BtAppEvent ev = {0};
    fill_event_common(&ev, frame);
    ev.type = BT_APP_EVENT_UNKNOWN_DID;
    log_w("BT_APP 未识别数据标识 0x%04X (ctrl=0x%02X)", frame->data_id,
          frame->ctrl);
    emit_event(&ev);
    break;
  }
  }
}

static bool bt_app_proto_init(void) {
  bt_app_parser_init(&s_parser, on_frame_received, NULL);
  log_i("BT_APP 应用协议初始化完成");
  return true;
}

static ProtocolResult bt_app_proto_parse(uint8_t *data, uint16_t len) {
  if (data == NULL || len == 0) {
    return PROTOCOL_RESULT_ERROR;
  }
  bt_app_parser_feed(&s_parser, data, len);
  /* 流式解析无法精确判断"剩余字节属于下一帧还是无效"，统一返回 OK */
  return PROTOCOL_RESULT_OK;
}

/**
 * @brief 通用发送入口 — cmd = 数据标识；param 指向 payload 缓冲(可为NULL)
 *        默认按"上行写应答 (ctrl=0x84)"发送，业务侧需要更细控制时直接调用
 *        BT_APP_Send* 高层 API。
 */
static bool bt_app_proto_send_cmd(uint16_t cmd, void *param) {
  (void)param; /* 简化签名：默认仅发送空 payload 的应答帧，业务自行精确发送 */
  if (s_send_func == NULL) {
    log_e("BT_APP send: send_func 未绑定");
    return false;
  }
  size_t n = bt_app_build_frame(s_tx_buffer, sizeof(s_tx_buffer),
                                NULL, /* 默认全 0xFF 地址 */
                                BT_APP_CTRL_WRITE_RSP, NULL,
                                BT_APP_DEV_TYPE_DEFAULT, cmd, 0, /* frame_seq */
                                NULL, 0);
  if (n == 0) {
    log_e("BT_APP send: build 失败 (cmd=0x%04X)", cmd);
    return false;
  }
  s_send_func(s_tx_buffer, (uint16_t)n);
  return true;
}

static void bt_app_proto_on_response(uint16_t code, const uint8_t *data,
                                     uint16_t len) {
  (void)data;
  (void)len;
  log_d("BT_APP on_response: 0x%04X", code);
}

static void bt_app_proto_set_send_func(ProtocolSendFunc f) { s_send_func = f; }

static void bt_app_proto_set_event_callback(ProtocolEventCallback cb) {
  s_proto_event_cb = cb;
}

const ProtocolInterface bt_app_protocol = {
    .name = "bt_app_proto",
    .init = bt_app_proto_init,
    .parse = bt_app_proto_parse,
    .send_cmd = bt_app_proto_send_cmd,
    .on_response = bt_app_proto_on_response,
    .set_send_func = bt_app_proto_set_send_func,
    .set_event_callback = bt_app_proto_set_event_callback,
    .preamble = NULL, /* preamble 已在 build_frame 内部生成 */
};

/* ============================================================================
 *                       业务侧 API 实现
 * ==========================================================================*/

void BT_APP_SetEventCallback(BtAppEventCallback cb) { s_app_event_cb = cb; }

const char *BT_APP_GetEventName(BtAppEventType type) {
  switch (type) {
  case BT_APP_EVENT_NONE:
    return "无事件";
  case BT_APP_EVENT_TRIG_TEST_DOWN:
    return "BB01 触发测试-下行";
  case BT_APP_EVENT_TRIG_TEST_UP:
    return "BB01 触发测试-上行";
  case BT_APP_EVENT_PAIR_SET_DOWN:
    return "BB02 写配对-下行";
  case BT_APP_EVENT_PAIR_SET_UP:
    return "BB02 写配对-上行";
  case BT_APP_EVENT_PAIR_READ_DOWN:
    return "BB02 读配对-下行";
  case BT_APP_EVENT_PAIR_READ_UP:
    return "BB02 读配对-上行";
  case BT_APP_EVENT_DEV_LIST_SYNC_DOWN:
    return "AAFF 设备列表-下行";
  case BT_APP_EVENT_DEV_LIST_SYNC_UP:
    return "AAFF 设备列表-上行";
  case BT_APP_EVENT_UNKNOWN_DID:
    return "未知数据标识";
  case BT_APP_EVENT_PARSE_ERROR:
    return "解析错误";
  default:
    return "未定义";
  }
}

/** 内部统一打包并发送 */
static bool bt_app_send_with_ctrl(const uint8_t target_addr[6],
                                  const uint8_t bcd_time[6], uint8_t ctrl,
                                  uint16_t data_id, const uint8_t *payload,
                                  uint16_t payload_len) {
  if (s_send_func == NULL) {
    log_e("BT_APP send: send_func 未绑定");
    return false;
  }
  size_t n = bt_app_build_frame(s_tx_buffer, sizeof(s_tx_buffer), target_addr,
                                ctrl, bcd_time, BT_APP_DEV_TYPE_DEFAULT,
                                data_id, 0, payload, payload_len);
  if (n == 0) {
    log_e("BT_APP send: build 失败 did=0x%04X ctrl=0x%02X len=%u", data_id,
          ctrl, payload_len);
    return false;
  }
  log_d("BT_APP TX did=0x%04X ctrl=0x%02X frame_len=%u", data_id, ctrl,
        (unsigned)n);
  s_send_func(s_tx_buffer, (uint16_t)n);
  return true;
}

bool BT_APP_SendTrigTestUp(const uint8_t target_addr[6],
                           const uint8_t bcd_time[6],
                           const uint8_t report_name[21]) {
  if (report_name == NULL) {
    return false;
  }
  return bt_app_send_with_ctrl(target_addr, bcd_time, BT_APP_CTRL_WRITE_RSP,
                               BT_APP_DID_TRIG_TEST, report_name, 21);
}

bool BT_APP_SendPairSetResultUp(const uint8_t target_addr[6],
                                const uint8_t bcd_time[6], uint8_t result,
                                const uint8_t *dev_list, uint8_t dev_count) {
  /* payload = 1B result + 1B count + N × (1B type + 24B id) */
  const uint16_t per = 1u + 24u;
  const uint16_t list_len = (uint16_t)(dev_count * per);
  const uint16_t need = (uint16_t)(2u + list_len);
  if (need > (BT_APP_MAX_DATA_LEN - BT_APP_DATA_HEAD_LEN)) {
    log_e("BT_APP PairSetUp: 设备数过多 %u", dev_count);
    return false;
  }
  uint8_t payload[BT_APP_MAX_DATA_LEN - BT_APP_DATA_HEAD_LEN];
  payload[0] = result;
  payload[1] = dev_count;
  if (dev_count && dev_list) {
    memcpy(&payload[2], dev_list, list_len);
  }
  return bt_app_send_with_ctrl(target_addr, bcd_time, BT_APP_CTRL_WRITE_RSP,
                               BT_APP_DID_PAIR_INFO, payload, need);
}

bool BT_APP_SendPairReadResultUp(const uint8_t target_addr[6],
                                 const uint8_t bcd_time[6],
                                 const uint8_t *dev_list, uint8_t dev_count) {
  /* payload = 1B count + N × (1B type + 1B status + 24B id + 6B mac) */
  const uint16_t per = 1u + 1u + 24u + 6u;
  const uint16_t list_len = (uint16_t)(dev_count * per);
  const uint16_t need = (uint16_t)(1u + list_len);
  if (need > (BT_APP_MAX_DATA_LEN - BT_APP_DATA_HEAD_LEN)) {
    log_e("BT_APP PairReadUp: 设备数过多 %u", dev_count);
    return false;
  }
  uint8_t payload[BT_APP_MAX_DATA_LEN - BT_APP_DATA_HEAD_LEN];
  payload[0] = dev_count;
  if (dev_count && dev_list) {
    memcpy(&payload[1], dev_list, list_len);
  }
  return bt_app_send_with_ctrl(target_addr, bcd_time, BT_APP_CTRL_READ_RSP,
                               BT_APP_DID_PAIR_INFO, payload, need);
}

bool BT_APP_SendDevListSyncUp(const uint8_t target_addr[6],
                              const uint8_t bcd_time[6],
                              const uint8_t *scan_blob, uint16_t scan_len) {
  if (scan_len > (BT_APP_MAX_DATA_LEN - BT_APP_DATA_HEAD_LEN)) {
    log_e("BT_APP DevListUp: 扫描数据过长 %u", scan_len);
    return false;
  }
  return bt_app_send_with_ctrl(target_addr, bcd_time, BT_APP_CTRL_READ_RSP,
                               BT_APP_DID_DEV_LIST_SYNC, scan_blob, scan_len);
}
