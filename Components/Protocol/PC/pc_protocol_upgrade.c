/**
 * @file pc_protocol_upgrade.c
 * @brief APP升级协议实现 (带魔数验证)
 * @version 2.0.0
 * @date 2026-01-05
 *
 * @section intro 简介
 * 实现APP固件升级相关的协议处理。
 * 支持手动模式和自动模式升级。
 * v2.0.0: 添加4字节魔数验证，支持多芯片平台
 *
 * @section protocol 协议格式
 * 升级命令 (0xBA):
 *   帧头: 0x68
 *   功能码: 0xBA
 *   长度: 0x11 (17字节)
 *   魔数: 4字节 (0xF7 + 厂商 + 芯片型号)
 *   工位号: 1字节
 *   升级模式: 0x00=手动, 0x01=自动
 *   波特率配置: 0x00=9600, 0x01=115200
 *   协议类型: 0x00=Xmodem
 *   超时时间: 秒数
 *   日志开关: 0x00=关, 0x01=开
 *   固件大小: 2字节(小端), 单位KB
 *   校验和: 累加和
 *   帧尾: 0x16
 *
 * 升级应答 (0xBB):
 *   帧头: 0x68
 *   功能码: 0xBB
 *   长度: 0x0B (11字节)
 *   魔数: 4字节 (0xF7 + 厂商 + 芯片型号)
 *   工位号: 1字节
 *   状态: 0x00=准备就绪, 0x01=参数错误, 0x02=忙, 0x03=固件超限, 0x04=魔数错误
 *   校验和: 累加和
 *   帧尾: 0x16
 */

#define LOG_TAG "pc_upgrade"

#include "../upgrade_magic.h"
#include "Utility/utility.h"
#include "pc_protocol.h"
#include <elog.h>
#include <string.h>

/*============ 外部依赖 ============*/

#include "../upgrade_storage.h"
#include "main.h"

// Bootloader跳转相关 - 需要在其他地方实现
extern void system_reset_to_bootloader(void) __attribute__((weak));
extern void save_upgrade_params_to_flash(void *params) __attribute__((weak));

/*============ 协议结构体 ============*/

#pragma pack(1)

/**
 * @brief 升级命令帧结构 (v2.0 带魔数)
 */
typedef struct {
  uint8_t head;         // [0] 0x68
  uint8_t cmd;          // [1] 0xBA
  uint8_t length;       // [2] 帧长度 = 0x11 (17字节)
  UpgradeMagic_t magic; // [3-6] 魔数: 0xF7 + 厂商 + 芯片型号
  uint8_t station_id;   // [7] 工位号
  uint8_t mode;         // [8] 升级模式: 0=手动, 1=自动
  uint8_t baud_cfg;     // [9] 波特率: 0=9600, 1=115200
  uint8_t protocol;     // [10] 协议: 0=Xmodem
  uint8_t timeout_sec;  // [11] 超时时间(秒)
  uint8_t log_enable;   // [12] 日志: 0=关, 1=开
  uint16_t fw_size_kb;  // [13-14] 固件大小(KB), 小端
  uint8_t checksum;     // [15] 校验和
  uint8_t tail;         // [16] 0x16
} UpgradeCommandFrame;

/**
 * @brief 升级应答帧结构 (v2.0 带魔数)
 */
typedef struct {
  uint8_t head;         // [0] 0x68
  uint8_t cmd;          // [1] 0xBB
  uint8_t length;       // [2] 帧长度 = 0x0B (11字节)
  UpgradeMagic_t magic; // [3-6] 魔数: 0xF7 + 厂商 + 芯片型号
  uint8_t station_id;   // [7] 工位号
  uint8_t status; // [8] 状态: 0=就绪, 1=参数错误, 2=忙, 3=固件超限, 4=魔数错误
  uint8_t checksum; // [9] 校验和
  uint8_t tail;     // [10] 0x16
} UpgradeResponseFrame;

#pragma pack()

// 静态断言检查结构体大小
_Static_assert(sizeof(UpgradeCommandFrame) == 17,
               "UpgradeCommandFrame size error! Expected 17 bytes");
_Static_assert(sizeof(UpgradeResponseFrame) == 11,
               "UpgradeResponseFrame size error! Expected 11 bytes");

/*============ 升级状态码 ============*/

typedef enum {
  UPGRADE_STATUS_READY = 0x00,         // 准备就绪，即将重启进入Bootloader
  UPGRADE_STATUS_PARAM_ERROR = 0x01,   // 参数错误
  UPGRADE_STATUS_BUSY = 0x02,          // 系统忙（正在测试中）
  UPGRADE_STATUS_SIZE_ERROR = 0x03,    // 固件大小超限
  UPGRADE_STATUS_MAGIC_ERROR = 0x04,   // 魔数错误（芯片不匹配）
  UPGRADE_STATUS_MAGIC_INVALID = 0x05, // 魔数无效（格式错误）
} UpgradeStatus;

/*============ 内部变量 ============*/

static ProtocolSendFunc s_send_func = NULL;
static ProtocolEventCallback s_event_callback = NULL;

// 升级参数存储
static UpgradeCommandFrame s_pending_upgrade = {0};
static bool s_upgrade_pending = false;

// Flash最大大小限制 (KB)
#define MAX_FW_SIZE_KB 256 // 256KB

/*============ 内部函数声明 ============*/

static bool upgrade_init(void);
static ProtocolResult upgrade_parse(uint8_t *data, uint16_t len);
static bool upgrade_send_cmd(uint16_t cmd, void *param);
static void upgrade_on_response(uint16_t code, const uint8_t *data,
                                uint16_t len);
static void upgrade_set_send_func(ProtocolSendFunc func);
static void upgrade_set_event_callback(ProtocolEventCallback callback);

// 内部处理函数
static void handle_upgrade_command(const uint8_t *data, uint16_t len);
static void send_upgrade_response(uint8_t status);

/*============ 协议接口实例 ============*/

const ProtocolInterface upgrade_pc_protocol = {
    .name = "upgrade",
    .init = upgrade_init,
    .parse = upgrade_parse,
    .send_cmd = upgrade_send_cmd,
    .on_response = upgrade_on_response,
    .set_send_func = upgrade_set_send_func,
    .set_event_callback = upgrade_set_event_callback,
};

/*============ 接口实现 ============*/

static bool upgrade_init(void) {
  log_i("APP升级协议初始化");
  s_upgrade_pending = false;
  memset(&s_pending_upgrade, 0, sizeof(s_pending_upgrade));
  return true;
}

/**
 * @brief 解析升级命令
 */
static ProtocolResult upgrade_parse(uint8_t *data, uint16_t len) {
  uint16_t pos = 0;
  bool handled = false;

  log_d("升级协议解析, 长度=%d", len);

  while (pos + 5 < len) {
    // 查找帧头
    if (data[pos] != FT_FRAME_HEAD) {
      pos++;
      continue;
    }

    uint8_t cmd = data[pos + 1];
    uint8_t frame_len = data[pos + 2];

    // 验证长度
    if (pos + frame_len > len) {
      log_d("帧不完整");
      return PROTOCOL_RESULT_INCOMPLETE;
    }

    // 验证帧尾
    if (data[pos + frame_len - 1] != FT_FRAME_TAIL) {
      pos++;
      continue;
    }

    // 只处理升级命令 0xBA
    if (cmd == PC_CMD_UPGRADE) {
      log_i("收到升级命令");
      handle_upgrade_command(&data[pos], frame_len);
      handled = true;
    }

    pos += frame_len;
  }

  // 只有真正处理了升级命令才返回OK，否则返回UNKNOWN_CMD让其他协议处理
  return handled ? PROTOCOL_RESULT_OK : PROTOCOL_RESULT_UNKNOWN_CMD;
}

/**
 * @brief 发送升级相关命令
 */
static bool upgrade_send_cmd(uint16_t cmd, void *param) {
  switch (cmd) {
  case PC_CMD_UPGRADE_ACK: {
    uint8_t status = param ? *(uint8_t *)param : UPGRADE_STATUS_READY;
    send_upgrade_response(status);
    return true;
  }
  default:
    log_w("升级协议: 未支持的命令 0x%04X", cmd);
    return false;
  }
}

static void upgrade_on_response(uint16_t code, const uint8_t *data,
                                uint16_t len) {
  log_d("升级协议: 收到响应 0x%04X", code);
}

static void upgrade_set_send_func(ProtocolSendFunc func) { s_send_func = func; }

static void upgrade_set_event_callback(ProtocolEventCallback callback) {
  s_event_callback = callback;
}

/*============ 内部处理函数 ============*/

/**
 * @brief 处理升级命令 (带魔数验证)
 */
static void handle_upgrade_command(const uint8_t *data, uint16_t len) {
  if (len < sizeof(UpgradeCommandFrame)) {
    log_e("升级命令帧长度错误: %d < %d", len, (int)sizeof(UpgradeCommandFrame));
    send_upgrade_response(UPGRADE_STATUS_PARAM_ERROR);
    return;
  }

  const UpgradeCommandFrame *frame = (const UpgradeCommandFrame *)data;

  // 验证帧长度
  if (frame->length != sizeof(UpgradeCommandFrame)) {
    log_e("升级命令长度字段错误: %d, 期望: %d", frame->length,
          (int)sizeof(UpgradeCommandFrame));
    send_upgrade_response(UPGRADE_STATUS_PARAM_ERROR);
    return;
  }

  // ===== 魔数验证 (新增) =====
  // 1. 验证魔数格式是否有效
  if (!Upgrade_ValidateMagic(&frame->magic)) {
    log_e("升级命令魔数无效: prefix=0x%02X vendor=0x%02X chip=0x%04X",
          frame->magic.prefix, frame->magic.vendor, frame->magic.chip);
    send_upgrade_response(UPGRADE_STATUS_MAGIC_INVALID);
    return;
  }

  // 2. 验证魔数是否匹配当前芯片
  if (!Upgrade_MatchCurrentChip(&frame->magic)) {
    const ChipInfo_t *current = Upgrade_GetCurrentChipInfo();
    const ChipInfo_t *target = Upgrade_FindChipInfo(&frame->magic);

    log_e("升级命令芯片不匹配!");
    log_e("  当前芯片: %s (%s, 0x%04X)", current->name,
          Upgrade_GetVendorName(current->vendor_code), current->chip_code);
    if (target) {
      log_e("  目标芯片: %s (%s, 0x%04X)", target->name,
            Upgrade_GetVendorName(target->vendor_code), target->chip_code);
    } else {
      log_e("  目标芯片: 未知 (厂商=0x%02X, 型号=0x%04X)", frame->magic.vendor,
            frame->magic.chip);
    }
    send_upgrade_response(UPGRADE_STATUS_MAGIC_ERROR);
    return;
  }

  log_i("魔数验证通过: %s (%s)", Upgrade_GetCurrentChipInfo()->name,
        Upgrade_GetVendorName(frame->magic.vendor));

  // 验证工位
  uint8_t local_station = PC_Protocol_GetStationId();
  if (frame->station_id != local_station) {
    log_d("工位不匹配: 收到%d, 本机%d", frame->station_id, local_station);
    return; // 不是发给本工位的，静默忽略
  }

  // 验证校验和 (从帧头累加到校验和前，与MES协议一致)
  uint8_t calc_sum = util_checksum_sum8(data, len - 2);
  if (calc_sum != frame->checksum) {
    log_e("升级命令校验和错误: 计算=0x%02X, 接收=0x%02X", calc_sum,
          frame->checksum);
    send_upgrade_response(UPGRADE_STATUS_PARAM_ERROR);
    return;
  }

  // 验证固件大小
  if (frame->fw_size_kb > MAX_FW_SIZE_KB) {
    log_e("固件大小超限: %dKB > %dKB", frame->fw_size_kb, MAX_FW_SIZE_KB);
    send_upgrade_response(UPGRADE_STATUS_SIZE_ERROR);
    return;
  }

  // 打印升级参数
  log_i("========================================");
  log_i("          APP升级参数确认");
  log_i("========================================");
  log_i("  魔数: 0x%02X %02X %02X %02X", frame->magic.prefix,
        frame->magic.vendor, (uint8_t)(frame->magic.chip & 0xFF),
        (uint8_t)((frame->magic.chip >> 8) & 0xFF));
  log_i("  芯片: %s", Upgrade_GetCurrentChipInfo()->name);
  log_i("  工位号: %d", frame->station_id);
  log_i("  升级模式: %s", frame->mode == 0 ? "手动" : "自动");
  log_i("  波特率: %s", frame->baud_cfg == 0 ? "9600" : "115200");
  log_i("  传输协议: %s", frame->protocol == 0 ? "Xmodem" : "未知");
  log_i("  超时时间: %d秒", frame->timeout_sec);
  log_i("  Bootloader日志: %s", frame->log_enable ? "开启" : "关闭");
  log_i("  固件大小: %dKB", frame->fw_size_kb);
  log_i("========================================");

  // 保存升级参数
  memcpy(&s_pending_upgrade, frame, sizeof(UpgradeCommandFrame));
  s_upgrade_pending = true;

  // 发送就绪应答
  send_upgrade_response(UPGRADE_STATUS_READY);

  // 保存参数到Flash (如果实现了)
  if (save_upgrade_params_to_flash) {
    save_upgrade_params_to_flash(&s_pending_upgrade);
    log_i("升级参数已保存到Flash");

    // 验证: 读取并打印Flash中的数据
    UpgradeStorageData_t verify_data;
    if (UpgradeStorage_ReadParams(&verify_data)) {
      log_i("========== Flash验证 (0x3E000) ==========");
      log_i("  Magic: 0x%08lX (期望: 0x55AA55AA)",
            (unsigned long)verify_data.magic);
      log_i("  Version: %d", verify_data.version);
      log_i("  工位号: %d", verify_data.station_id);
      log_i("  升级模式: %d", verify_data.upgrade_mode);
      log_i("  波特率: %d", verify_data.baud_config);
      log_i("  升级标志: %d (1=升级)", verify_data.upgrade_flag);
      log_i("=========================================");
    } else {
      log_w("无法读取Flash验证数据");
    }
  }

  // 触发事件回调
  if (s_event_callback) {
    s_event_callback(PROTOCOL_EVENT_UPGRADE_REQUEST, PC_CMD_UPGRADE,
                     (const uint8_t *)frame, sizeof(*frame));
  }

  log_i("准备重启进入Bootloader...");

  // 延时一小段时间确保应答发送完成
  // 然后重启进入Bootloader
  // 注意：这里需要实际实现重启函数
  if (system_reset_to_bootloader) {
    // 给一些时间让应答发送完成
    // 实际代码中可能需要用定时器延迟执行
    system_reset_to_bootloader();
  } else {
    log_w("system_reset_to_bootloader() 未实现");
  }
}

/**
 * @brief 发送升级应答 (带魔数)
 */
static void send_upgrade_response(uint8_t status) {
  UpgradeResponseFrame frame = {
      .head = FT_FRAME_HEAD,
      .cmd = PC_CMD_UPGRADE_ACK,
      .length = sizeof(UpgradeResponseFrame),
      .magic =
          {
              .prefix = UPGRADE_MAGIC_PREFIX,
              .vendor = CURRENT_CHIP_VENDOR,
              .chip = CURRENT_CHIP_CODE,
          },
      .station_id = PC_Protocol_GetStationId(),
      .status = status,
      .checksum = 0,
      .tail = FT_FRAME_TAIL,
  };

  // 计算校验和 (从帧头累加到校验和前，与MES协议一致)
  frame.checksum = util_checksum_sum8((uint8_t *)&frame, sizeof(frame) - 2);

  const char *status_str = "未知";
  switch (status) {
  case UPGRADE_STATUS_READY:
    status_str = "准备就绪";
    break;
  case UPGRADE_STATUS_PARAM_ERROR:
    status_str = "参数错误";
    break;
  case UPGRADE_STATUS_BUSY:
    status_str = "系统忙";
    break;
  case UPGRADE_STATUS_SIZE_ERROR:
    status_str = "固件超限";
    break;
  case UPGRADE_STATUS_MAGIC_ERROR:
    status_str = "芯片不匹配";
    break;
  case UPGRADE_STATUS_MAGIC_INVALID:
    status_str = "魔数无效";
    break;
  }
  log_i("发送升级应答: %s (0x%02X)", status_str, status);

  if (s_send_func != NULL) {
    s_send_func((uint8_t *)&frame, sizeof(frame));
  }
}

/*============ 公共API ============*/

/**
 * @brief 检查是否有待处理的升级请求
 */
bool Upgrade_IsPending(void) { return s_upgrade_pending; }

/**
 * @brief 获取待处理的升级参数
 */
bool Upgrade_GetPendingParams(uint8_t *mode, uint8_t *baud_cfg,
                              uint8_t *timeout_sec, uint16_t *fw_size_kb) {
  if (!s_upgrade_pending) {
    return false;
  }

  if (mode)
    *mode = s_pending_upgrade.mode;
  if (baud_cfg)
    *baud_cfg = s_pending_upgrade.baud_cfg;
  if (timeout_sec)
    *timeout_sec = s_pending_upgrade.timeout_sec;
  if (fw_size_kb)
    *fw_size_kb = s_pending_upgrade.fw_size_kb;

  return true;
}

/**
 * @brief 清除升级请求
 */
void Upgrade_ClearPending(void) {
  s_upgrade_pending = false;
  memset(&s_pending_upgrade, 0, sizeof(s_pending_upgrade));
}
