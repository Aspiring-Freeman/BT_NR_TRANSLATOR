/**
 * @file    bt_app_proto.h
 * @brief   蓝牙透传应用层协议解析/构建
 *          帧格式：FE FE FE | 68 | addr[6] | 68 | ctrl | len[2,LE] | data[len]
 * | cksum | 16 数据域(data) = bcd_time[6] + dev_type[1] + data_id[2,BE] +
 * frame_seq[1] + payload[len-10]
 * @note    DS809 BLE 模组之上的应用协议层，与底层透传驱动 bt_ds809 解耦。
 *          payload 部分按数据标识可能为 AES 加密，由上层处理。
 */
#ifndef BT_APP_PROTO_H
#define BT_APP_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol_def.h" /* ProtocolInterface, ProtocolSendFunc */

#ifdef __cplusplus
extern "C" {
#endif

/* ===== 协议常量 ===== */
#define BT_APP_PREAMBLE_BYTE 0xFEu
#define BT_APP_PREAMBLE_LEN 3u /* 发送时固定 3 个；接收时容忍任意数量 */
#define BT_APP_START_BYTE 0x68u
#define BT_APP_END_BYTE 0x16u
#define BT_APP_TARGET_ADDR_LEN 6u

/* 控制码 */
#define BT_APP_CTRL_READ_REQ 0x01u
#define BT_APP_CTRL_WRITE_REQ 0x04u
#define BT_APP_CTRL_READ_RSP 0x81u
#define BT_APP_CTRL_WRITE_RSP 0x84u
#define BT_APP_CTRL_RSP_FLAG 0x80u /* bit7: 1=上行响应 */

/* 数据标识 (高字节在前组合) */
#define BT_APP_DID_TRIG_TEST 0xBB01u     /* 触发通讯启动及测试 */
#define BT_APP_DID_PAIR_INFO 0xBB02u     /* 设置/读取配对信息 */
#define BT_APP_DID_DEV_LIST_SYNC 0xAAFFu /* 设备列表同步信息 */

/* 设备类型 */
#define BT_APP_DEV_TYPE_DEFAULT 0x02u

/* 数据域固定头长度: time[6] + dev_type[1] + data_id[2] + frame_seq[1] */
#define BT_APP_DATA_HEAD_LEN 10u

#ifndef BT_APP_MAX_DATA_LEN
#define BT_APP_MAX_DATA_LEN 256u /* 数据域最大字节数；可外部覆盖 */
#endif

/* 帧固定开销 (preamble + start1 + addr + start2 + ctrl + len + cksum + end) */
#define BT_APP_FIXED_OVERHEAD                                                  \
  (BT_APP_PREAMBLE_LEN + 1u + BT_APP_TARGET_ADDR_LEN + 1u + 1u + 2u + 1u + 1u)
#define BT_APP_MAX_FRAME_LEN (BT_APP_FIXED_OVERHEAD + BT_APP_MAX_DATA_LEN)

/* 内部缓存：从 start1 到 数据域末尾 (不含 cksum/end) */
#define BT_APP_BUF_SIZE                                                        \
  (1u + BT_APP_TARGET_ADDR_LEN + 1u + 1u + 2u + BT_APP_MAX_DATA_LEN)

/* ===== 帧视图 (回调里使用) ===== */
typedef struct {
  uint8_t target_addr[BT_APP_TARGET_ADDR_LEN];
  uint8_t ctrl;
  uint16_t data_len; /* 数据域总长 (含 10 字节固定头) */
  /* --- 数据域 --- */
  uint8_t bcd_time[6]; /* 年月日时分秒，BCD */
  uint8_t dev_type;
  uint16_t data_id;       /* 例如 0xBB01 */
  uint8_t frame_seq;      /* 0 = 无多帧 */
  const uint8_t *payload; /* 指向数据内容(可能加密)；NULL 表示无 */
  uint16_t payload_len;   /* = data_len - 10 */
} bt_app_frame_t;

typedef void (*bt_app_frame_cb_t)(const bt_app_frame_t *frame, void *user);

/* ===== 解析器实例 ===== */
typedef struct {
  uint8_t state;
  uint16_t addr_cnt;
  uint16_t data_cnt;
  uint16_t data_len; /* 期望的数据域长度 */
  uint8_t sum;       /* 运行累加和 (start1 起) */
  uint16_t buf_idx;
  uint8_t buf[BT_APP_BUF_SIZE];
  bt_app_frame_cb_t cb;
  void *cb_user;
} bt_app_parser_t;

/* ===== 接口 ===== */

/** 初始化解析器，注册整帧到达回调 */
void bt_app_parser_init(bt_app_parser_t *p, bt_app_frame_cb_t cb, void *user);

/** 复位状态机 (例如连接断开/重连时调用) */
void bt_app_parser_reset(bt_app_parser_t *p);

/** 喂入字节流；可任意分包，状态机会自动同步 */
void bt_app_parser_feed(bt_app_parser_t *p, const uint8_t *data, size_t len);

/**
 * 构建一帧待发送数据。
 * @param  target_addr  目标地址；NULL → 全 0xFF
 * @param  bcd_time     6 字节 BCD 时间；NULL → 全 0x00
 * @param  data_id      MSB-first 组合的数据标识
 * @param  payload      数据内容指针 (已加密)；可为 NULL
 * @param  payload_len  数据内容长度；上限 BT_APP_MAX_DATA_LEN - 10
 * @return 实际写入字节数；0 = 参数错或 buf 不足
 */
size_t bt_app_build_frame(uint8_t *buf, size_t buf_size,
                          const uint8_t target_addr[BT_APP_TARGET_ADDR_LEN],
                          uint8_t ctrl, const uint8_t bcd_time[6],
                          uint8_t dev_type, uint16_t data_id, uint8_t frame_seq,
                          const uint8_t *payload, uint16_t payload_len);

/* ===== BCD 辅助 ===== */
static inline uint8_t bt_app_bcd2dec(uint8_t b) {
  return (uint8_t)(((b >> 4) * 10u) + (b & 0x0Fu));
}
static inline uint8_t bt_app_dec2bcd(uint8_t d) {
  return (uint8_t)(((d / 10u) << 4) | (d % 10u));
}

/* ===========================================================================
 *  事件层 (与 bt_ds809 风格一致)
 * ===========================================================================
 *  低层 bt_app_parser_t 抽出整帧后，由本模块的 ProtocolInterface 适配为
 *  BtAppEvent 上抛给业务层 (ble.c / 业务 handler)。
 * =========================================================================*/

/** 事件类型 (按数据标识 + 控制码方向区分) */
typedef enum {
  BT_APP_EVENT_NONE = 0,

  /* 0xBB01 触发通讯启动及测试 */
  BT_APP_EVENT_TRIG_TEST_DOWN, /* APP→工装→报警器 (ctrl=0x04) */
  BT_APP_EVENT_TRIG_TEST_UP,   /* 报警器→工装→APP (ctrl=0x84) */

  /* 0xBB02 设置/读取配对信息 */
  BT_APP_EVENT_PAIR_SET_DOWN,  /* 写下行 ctrl=0x04 */
  BT_APP_EVENT_PAIR_SET_UP,    /* 写上行 ctrl=0x84 */
  BT_APP_EVENT_PAIR_READ_DOWN, /* 读下行 ctrl=0x01 */
  BT_APP_EVENT_PAIR_READ_UP,   /* 读上行 ctrl=0x81 */

  /* 0xAAFF 设备列表同步 */
  BT_APP_EVENT_DEV_LIST_SYNC_DOWN, /* APP→工装 ctrl=0x01 */
  BT_APP_EVENT_DEV_LIST_SYNC_UP,   /* 工装→APP ctrl=0x81 */

  /* 错误/未识别 */
  BT_APP_EVENT_UNKNOWN_DID, /* 未识别的数据标识 */
  BT_APP_EVENT_PARSE_ERROR, /* 校验和或长度错误 */

  BT_APP_EVENT_MAX,
} BtAppEventType;

/* ---- 各事件携带的解析后字段 ---- */

/** 0xBB01 上行 — 报警器编号 (ASCII, 21B 定长) */
typedef struct {
  uint8_t report_name[21];
} BtAppTrigTestUpData;

/** 0xBB02 写下行 — 待配对设备列表  (1B 类型 + 24B 编号) × N */
typedef struct {
  uint8_t dev_count;
  const uint8_t *dev_list; /* 指向 frame->payload 内部，仅回调期间有效 */
} BtAppPairSetDownData;

/** 0xBB02 写上行 — 写入结果 + 已生效列表 */
typedef struct {
  uint8_t result; /* 0=成功 1=失败 */
  uint8_t dev_count;
  const uint8_t *dev_list; /* (1B 类型 + 24B 编号) × N */
} BtAppPairSetUpData;

/** 0xBB02 读上行 — 当前配对设备及其连接/MAC */
typedef struct {
  uint8_t dev_count;
  const uint8_t *dev_list; /* (1B 类型 + 1B 状态 + 24B 编号 + 6B MAC) × N */
} BtAppPairReadUpData;

/** 0xAAFF — 扫描列表 (上下行均使用相同结构) */
typedef struct {
  const uint8_t *scan_blob; /* 原始 (RSSI1 + MAC6 + 扫描n) × N */
  uint16_t scan_len;
} BtAppDevListData;

/** 事件结构 — DS809 风格：通用元数据 + union 携带各事件解析后字段 */
typedef struct {
  BtAppEventType type;

  /* ---- 通用元数据 (从帧头复制；payload 指向缓冲内部，仅回调期间有效) ---- */
  uint8_t target_addr[BT_APP_TARGET_ADDR_LEN];
  uint8_t ctrl;
  uint16_t data_id;
  uint8_t bcd_time[6];
  uint8_t dev_type;
  uint8_t frame_seq;
  const uint8_t *payload;
  uint16_t payload_len;

  /* ---- 原始帧视图 (备查) ---- */
  const bt_app_frame_t *frame;

  /* ---- 各事件解析后字段 ---- */
  union {
    BtAppTrigTestUpData trig_test_up;
    BtAppPairSetDownData pair_set_down;
    BtAppPairSetUpData pair_set_up;
    BtAppPairReadUpData pair_read_up;
    BtAppDevListData dev_list; /* DOWN/UP 共用 */
  } data;
} BtAppEvent;

/** 业务层事件回调 */
typedef void (*BtAppEventCallback)(const BtAppEvent *event);

/** 设备侧协议接口实例 (注册到 ProtocolManager 的 Device 槽) */
extern const ProtocolInterface bt_app_protocol;

/* ===== 业务侧 API (高层封装) ===== */

/**
 * @brief 注册业务层事件回调
 */
void BT_APP_SetEventCallback(BtAppEventCallback cb);

/**
 * @brief 获取事件名称 (调试日志)
 */
const char *BT_APP_GetEventName(BtAppEventType type);

/**
 * @brief 触发通讯启动及测试 (0xBB01) — 上行回复给 APP
 * @param target_addr  目标地址 (NULL = 全 0xFF)
 * @param bcd_time     6 字节 BCD 时间 (NULL = 全 0)
 * @param report_name  21 字节 ASCII 报警器编号
 */
bool BT_APP_SendTrigTestUp(const uint8_t target_addr[6],
                           const uint8_t bcd_time[6],
                           const uint8_t report_name[21]);

/**
 * @brief 设置配对信息应答 (0xBB02 写上行)
 * @param result  0=成功，1=失败
 * @param dev_list  设备数组，每项 (1B 类型 + 24B ASCII 编号)
 * @param dev_count 设备数量
 */
bool BT_APP_SendPairSetResultUp(const uint8_t target_addr[6],
                                const uint8_t bcd_time[6], uint8_t result,
                                const uint8_t *dev_list, uint8_t dev_count);

/**
 * @brief 读取配对信息应答 (0xBB02 读上行)
 * @param dev_list  设备数组，每项 (1B 类型 + 1B 连接状态 + 24B 编号 + 6B MAC)
 */
bool BT_APP_SendPairReadResultUp(const uint8_t target_addr[6],
                                 const uint8_t bcd_time[6],
                                 const uint8_t *dev_list, uint8_t dev_count);

/**
 * @brief 设备列表同步应答 (0xAAFF 上行)
 * @param scan_blob  扫描结果 (信号强度1 + MAC6 + 扫描数据n) × N
 */
bool BT_APP_SendDevListSyncUp(const uint8_t target_addr[6],
                              const uint8_t bcd_time[6],
                              const uint8_t *scan_blob, uint16_t scan_len);

#ifdef __cplusplus
}
#endif
#endif /* BT_APP_PROTO_H */
