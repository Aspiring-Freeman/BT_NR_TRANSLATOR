/**
 * @file pc_protocol_config.c
 * @brief 公共调试配置协议
 * @version 1.1.0
 * @date 2024-12
 *
 * @section intro 简介
 * 实现通用的调试配置协议，包括：
 * - 设置调试模式和透传模式 (0xAE)
 * - 查询当前测试步骤 (0xBE)
 *
 * 这些配置命令与具体的表计类型无关，是调试用的公共协议。
 * 所有表计类型（水表、膜式气表、超声波气表等）都可以使用。
 *
 * @section protocol 协议格式
 * 配置命令: 68 AE 09 [工位号] [调试模式] [透传模式] [透传前导] [校验和] 16
 * 配置应答: 68 AF 09 [工位号] [调试模式] [透传模式] [透传前导] [校验和] 16
 * 查询步骤: 68 BE 06 [工位号] [校验和] 16
 * 步骤应答: 68 BF [长度] [工位号] [测试状态] [失败原因] [步骤ID] [名称长度]
 * [步骤名称...] [原因长度] [原因名称...] [校验和] 16 测试状态: 0=进行中,
 * 1=成功, 2=失败 失败原因: 枚举值 (0=无失败)
 *
 * @note 透传前导: 0=无前导(膜表), 1=有前导(水表)
 */

#define LOG_TAG "pc_config"

#include "main.h"
#include "pc_protocol.h"
#include "build_info.h"
#include <elog.h>
#include <stdio.h>
#include <string.h>

/*============ 内部变量 ============*/

static ProtocolSendFunc s_send_func = NULL;
static ProtocolEventCallback s_event_callback = NULL;

// 发送缓冲区
#define CONFIG_TX_BUF_SIZE 128
static uint8_t s_tx_buffer[CONFIG_TX_BUF_SIZE];

/*============ 协议帧结构 ============*/

#pragma pack(1)
// 配置命令帧 (9字节)
typedef struct {
  uint8_t head;             // [0] 0x68 帧头
  uint8_t cmd;              // [1] 0xAE 命令
  uint8_t length;           // [2] 帧长度 (0x09)
  uint8_t station_id;       // [3] 工位号
  uint8_t debug_mode;       // [4] 调试模式: 0=关, 1=开
  uint8_t passthrough_mode; // [5] 透传模式: 0=关, 1=开
  uint8_t preamble_mode;    // [6] 透传前导: 0=无前导, 1=有前导
  uint8_t checksum;         // [7] 校验和
  uint8_t tail;             // [8] 0x16 帧尾
} ConfigCmdFrame;

// 配置应答帧 (9字节)
typedef struct {
  uint8_t head;             // [0] 0x68 帧头
  uint8_t cmd;              // [1] 0xAF 命令
  uint8_t length;           // [2] 帧长度 (0x09)
  uint8_t station_id;       // [3] 工位号
  uint8_t debug_mode;       // [4] 当前调试模式
  uint8_t passthrough_mode; // [5] 当前透传模式
  uint8_t preamble_mode;    // [6] 当前透传前导模式
  uint8_t checksum;         // [7] 校验和
  uint8_t tail;             // [8] 0x16 帧尾
} ConfigAckFrame;

// 查询步骤命令帧
typedef struct {
  uint8_t head;       // [0] FT_FRAME_HEAD 帧头
  uint8_t cmd;        // [1] 0xBE 命令
  uint8_t length;     // [2] 帧长度
  uint8_t station_id; // [3] 工位号
  uint8_t checksum;   // [4] 校验和
  uint8_t tail;       // [5] FT_FRAME_TAIL 帧尾
} QueryStepFrame;

// 查询程序版本号命令帧，PC发送指令示例，1工位：55 C0 06 01 1C AA
typedef struct {
  uint8_t head;       // [0] FT_FRAME_HEAD 帧头
  uint8_t cmd;        // [1] 0xC0 命令
  uint8_t length;     // [2] 帧长度
  uint8_t station_id; // [3] 工位号
  uint8_t checksum;   // [4] 校验和
  uint8_t tail;       // [5] FT_FRAME_TAIL 帧尾
} QueryConfigFrame;

// 查询程序版本号响应帧 (0xC1)
typedef struct {
  uint8_t head;        // [0] FT_FRAME_HEAD 帧头
  uint8_t cmd;         // [1] 0xC1 命令
  uint8_t length;      // [2] 帧长度
  uint8_t station_id;  // [3] 工位号
  char version[16];    // [4-19] 程序版本字符串
  char build_time[20]; // [20-39] 编译时间字符串
  uint8_t checksum;    // [40] 校验和
  uint8_t tail;        // [41] FT_FRAME_TAIL 帧尾
} QueryConfigAckFrame;

// 功能控制命令帧 (36字节)
// 功能：电源控制、功耗测试、电压采集、到位信号、霍尔控制等
typedef struct {
  uint8_t head;       // [0] FT_FRAME_HEAD 帧头
  uint8_t cmd;        // [1] 0xD0 命令
  uint8_t length;     // [2] 帧长度 (36)
  uint8_t station_id; // [3] 工位号

  // 功能1-2: 电源控制
  uint8_t main_power; // [4] 主电源 0关/1开/FF不操作
  uint8_t aux_power;  // [5] 从电源 0关/1开/FF不操作

  // 功能3: 静态功耗测试
  uint8_t pwr_test_en;    // [6] 功耗测试 0停/1正常/2低功耗
  uint8_t pwr_interval_l; // [7] 功耗采集间隔ms 低字节
  uint8_t pwr_interval_h; // [8] 功耗采集间隔ms 高字节
  uint8_t pwr_avg_cnt;    // [9] 功耗平均次数
  uint8_t pwr_print_int;  // [10] 功耗打印间隔秒
  uint8_t pwr_print_cnt;  // [11] 功耗打印次数 0=持续

  // 功能4: 阀门电压采集
  uint8_t valve_en;         // [12] 阀门电压采集使能
  uint8_t valve_interval_l; // [13] 阀门采集间隔ms 低字节
  uint8_t valve_interval_h; // [14] 阀门采集间隔ms 高字节
  uint8_t valve_avg_cnt;    // [15] 阀门平均次数
  uint8_t valve_print_int;  // [16] 阀门打印间隔秒
  uint8_t valve_print_cnt;  // [17] 阀门打印次数

  // 功能5: 各路电压显示
  uint8_t volt_en;         // [18] 各路电压使能
  uint8_t volt_interval_l; // [19] 各路电压采集间隔ms 低字节
  uint8_t volt_interval_h; // [20] 各路电压采集间隔ms 高字节
  uint8_t volt_avg_cnt;    // [21] 各路电压平均次数
  uint8_t volt_print_int;  // [22] 各路电压打印间隔秒
  uint8_t volt_print_cnt;  // [23] 各路电压打印次数

  // 功能6-7: 到位信号输出
  uint8_t pos1_en;  // [24] 到位信号1使能
  uint8_t pos1_dur; // [25] 到位信号1持续秒
  uint8_t pos2_en;  // [26] 到位信号2使能
  uint8_t pos2_dur; // [27] 到位信号2持续秒

  // 功能8-10: 霍尔/电磁铁控制
  uint8_t hall1_en;  // [28] 霍尔1使能
  uint8_t hall1_dur; // [29] 霍尔1持续秒 (≤15)
  uint8_t hall2_en;  // [30] 霍尔2使能
  uint8_t hall2_dur; // [31] 霍尔2持续秒
  uint8_t hall3_en;  // [32] 霍尔3使能
  uint8_t hall3_dur; // [33] 霍尔3持续秒

  uint8_t checksum; // [34] 校验和
  uint8_t tail;     // [35] FT_FRAME_TAIL 帧尾
} FTControlCmdFrame;
// 功能控制码响应
typedef struct {
  uint8_t head;       // [0] FT_FRAME_HEAD 帧头
  uint8_t cmd;        // [1] 0xD1 命令
  uint8_t length;     // [2] 帧长度
  uint8_t station_id; // [3] 工位号
  uint8_t status;     // [4] 状态，0成功，1失败
  uint8_t checksum;   // [5] 校验和
  uint8_t tail;       // [6] FT_FRAME_TAIL 帧尾
} FTControlAckFrame;
#pragma pack()

/*============ 内部函数声明 ============*/

static bool config_init(void);
static ProtocolResult config_parse(uint8_t *data, uint16_t len);
static bool config_send_cmd(uint16_t cmd, void *param);
static void config_on_response(uint16_t code, const uint8_t *data,
                               uint16_t len);
static void config_set_send_func(ProtocolSendFunc func);
static void config_set_event_callback(ProtocolEventCallback callback);

// 命令处理函数
static void handle_set_config(const uint8_t *data, uint16_t len);
static void handle_query_config(const uint8_t *data, uint16_t len);
static void handle_ft_control(const uint8_t *data, uint16_t len);
static void handle_query_fail_step(const uint8_t *data, uint16_t len);

// 响应发送函数
static void send_config_ack(void);
static void send_fail_step_response(void);

/*============ 协议接口实例 ============*/

const ProtocolInterface config_pc_protocol = {
    .name = "pc_config",
    .init = config_init,
    .parse = config_parse,
    .send_cmd = config_send_cmd,
    .on_response = config_on_response,
    .set_send_func = config_set_send_func,
    .set_event_callback = config_set_event_callback,
};

/*============ 接口实现 ============*/

static bool config_init(void) {
  log_i("公共调试配置协议初始化");
  return true;
}

static ProtocolResult config_parse(uint8_t *data, uint16_t len) {
  uint16_t pos = 0;
  bool handled = false;

  log_d("调试配置协议开始解析, 长度=%d", len);

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
      log_d("帧不完整, 需要%d字节", frame_len);
      return PROTOCOL_RESULT_INCOMPLETE;
    }

    // 验证帧尾
    if (data[pos + frame_len - 1] != FT_FRAME_TAIL) {
      log_d("帧尾错误");
      pos++;
      continue;
    }

    // 处理配置相关命令
    switch (cmd) {
    case PC_CMD_QUERY_CONFIG: // 0xC0
      log_d("收到查询配置命令");
      handle_query_config(&data[pos], frame_len);
      handled = true;
      break;
    case PC_CMD_FT_CONTROL: // 0x
      log_d("收到FT控制命令");
      handle_ft_control(&data[pos], frame_len);
      handled = true;
      break;
    case PC_CMD_SET_CONFIG: // 0xAE
      log_d("收到设置配置命令");
      handle_set_config(&data[pos], frame_len);
      handled = true;
      break;

    case PC_CMD_QUERY_FAIL_STEP: // 0xBE
      log_d("收到查询失败步骤命令");
      handle_query_fail_step(&data[pos], frame_len);
      handled = true;
      break;

    default:
      // 非配置命令，让其他协议处理
      return PROTOCOL_RESULT_UNKNOWN_CMD;
    }

    pos += frame_len;
  }

  return handled ? PROTOCOL_RESULT_OK : PROTOCOL_RESULT_UNKNOWN_CMD;
}

static bool config_send_cmd(uint16_t cmd, void *param) {
  switch (cmd) {
  case PC_CMD_SET_CONFIG_ACK:
    send_config_ack();
    return true;

  case PC_CMD_QUERY_FAIL_STEP_ACK:
    send_fail_step_response();
    return true;

  default:
    log_w("未支持的发送命令: 0x%04X", cmd);
    return false;
  }
}

static void config_on_response(uint16_t code, const uint8_t *data,
                               uint16_t len) {
  log_d("配置协议收到响应: 0x%04X", code);
}

static void config_set_send_func(ProtocolSendFunc func) { s_send_func = func; }

static void config_set_event_callback(ProtocolEventCallback callback) {
  s_event_callback = callback;
}

/*============ 命令处理实现 ============*/

/**
 * @brief 处理设置配置命令 (0xAE)
 *
 * 协议格式:
 *   请求: 68 AE 09 [工位号] [调试模式] [透传模式] [透传前导] [校验和] 16
 *   响应: 68 AF 09 [工位号] [调试模式] [透传模式] [透传前导] [校验和] 16
 *
 * @param data 帧数据
 * @param len  帧长度
 */
static void handle_set_config(const uint8_t *data, uint16_t len) {
  if (len < 9) {
    log_e("配置帧长度错误: %d < 9", len);
    return;
  }

  // 验证工位
  uint8_t station = data[3];
  uint8_t local_station = PC_Protocol_GetStationId();
  if (station != local_station) {
    log_d("工位不匹配: 收到%d, 本机%d", station, local_station);
    return;
  }

  // 验证校验和 (前7个字节)
  uint8_t calc_sum = 0;
  for (uint16_t i = 0; i < 7; i++) {
    calc_sum += data[i];
  }

  if (calc_sum != data[7]) {
    log_e("配置命令校验和错误: 计算=0x%02X, 接收=0x%02X", calc_sum, data[7]);
    return;
  }

  // 设置调试模式
  uint8_t old_debug = Debug_Mode;
  Debug_Mode = (data[4] != 0) ? 1 : 0;

  // 设置透传模式
  uint8_t old_passthrough = PassThrough_Mode;
  PassThrough_Mode = (data[5] != 0) ? 1 : 0;

  // 设置透传前导模式
  uint8_t old_preamble = PassThrough_Preamble;
  PassThrough_Preamble = (data[6] != 0) ? 1 : 0;

  // 打印配置变化
  log_i("+----------------------------------------------+");
  log_i("|          调试配置已更新                     |");
  log_i("+----------------------------------------------+");
  log_i("| 调试模式: %s -> %s", old_debug ? "开" : "关",
        Debug_Mode ? "开" : "关");
  log_i("| 透传模式: %s -> %s", old_passthrough ? "开" : "关",
        PassThrough_Mode ? "开" : "关");
  log_i("| 透传前导: %s -> %s", old_preamble ? "有" : "无",
        PassThrough_Preamble ? "有" : "无");
  log_i("+----------------------------------------------+");

  // 发送应答
  send_config_ack();

  // 触发事件回调
  if (s_event_callback != NULL) {
    s_event_callback(PROTOCOL_EVENT_RECEIVED, PC_CMD_SET_CONFIG, data, len);
  }
}

/**
 * @brief 处理查询配置命令 (0xC0)
 *
 * 协议格式:
 *   请求: FT_FRAME_HEAD C0 06 [工位号] [校验和] FT_FRAME_TAIL
 *   响应: FT_FRAME_HEAD C1 [长度] [工位号] [程序版本] [编译时间] ... [校验和]
 * FT_FRAME_TAIL
 *
 * @param data 帧数据
 * @param len  帧长度
 */
static void handle_query_config(const uint8_t *data, uint16_t len) {
  if (len < 6) {
    log_e("查询配置帧长度错误: %d < 6", len);
    return;
  }

  // 验证工位
  uint8_t station = data[3];
  uint8_t local_station = PC_Protocol_GetStationId();
  if (station != local_station) {
    log_d("工位不匹配: 收到%d, 本机%d", station, local_station);
    return;
  }

  // 验证校验和
  uint8_t calc_sum = 0;
  for (uint16_t i = 0; i < len - 2; i++) {
    calc_sum += data[i];
  }
  if (calc_sum != data[len - 2]) {
    log_e("查询配置校验和错误");
    return;
  }

  // 构建响应帧
  uint16_t pos = 0;
  s_tx_buffer[pos++] = FT_FRAME_HEAD;
  s_tx_buffer[pos++] = PC_CMD_QUERY_CONFIG_ACK; // 0xC1
  s_tx_buffer[pos++] = 0;                       // 长度占位

  // 工位号
  s_tx_buffer[pos++] = PC_Protocol_GetStationId();

  // 程序版本字符串,从main.c中定义的版本号获取,使用回调函数
  // 如果没有设置回调，返回默认值
  // 声明外部回调变量（定义在pc_protocol_common.c）
  extern uint16_t (*pc_get_version_func)(void);
  extern void (*pc_get_build_time_func)(char *build_time);

  char version_str[16] = "V0.0.0";
  if (pc_get_version_func != NULL) {
    uint16_t ver = pc_get_version_func();
    snprintf(version_str, sizeof(version_str), "V%d.%d.%d", ver >> 8,
             (ver >> 4) & 0x0F, ver & 0x0F);
  }
  uint8_t version_len = strlen(version_str);
  memcpy(&s_tx_buffer[pos], version_str, version_len);
  pos += version_len;

  // 编译时间字符串,使用回调函数
  // 如果没有设置回调，返回默认值
  char build_time_str[32] = "2000-01-01 00:00";
  if (pc_get_build_time_func != NULL) {
    pc_get_build_time_func(build_time_str);
  }
  uint8_t build_time_len = strlen(build_time_str);
  memcpy(&s_tx_buffer[pos], build_time_str, build_time_len);
  pos += build_time_len;

  // 设置长度字段
  s_tx_buffer[2] = pos - 3 + 2; // 减去头部3字节，加上校验和和尾部2字节

  // 计算校验和
  uint8_t checksum = 0;
  for (uint16_t i = 0; i < pos; i++) {
    checksum += s_tx_buffer[i];
  }
  s_tx_buffer[pos++] = checksum;

  // 帧尾
  s_tx_buffer[pos++] = FT_FRAME_TAIL;

  log_d("发送查询配置响应, 长度=%d", pos);

  // 统一打印一个清爵的版本/构建信息块, 方便现场诊断
  log_i("================ Build Info (0xC0) ================");
  log_i("  Version : %s", version_str);
  log_i("  Build   : %s", build_time_str);
  log_i("  Git     : %s @ %s%s", BUILD_GIT_BRANCH, BUILD_GIT_HASH,
        BUILD_GIT_DIRTY ? " (dirty)" : "");
  log_i("===================================================");

  // 发送响应
  if (s_send_func != NULL) {
    if (Debug_Mode != 0) {
      return;
    } else {
      s_send_func(s_tx_buffer, pos);
    }
  }
}

/**
 * @brief
 * 处理夹具控制命令,比如开启电源，关闭电源等，测试正常功耗，测试低功耗等等不同的需求
 *
 * 协议格式:
 *   请求: FT_FRAME_HEAD FT_control_cmd ?? [工位号] [命令数据...] [校验和]
 * FT_FRAME_TAIL 响应: FT_FRAME_HEAD FT_control_cmd ?? [工位号] [响应数据...]
 * [校验和] FT_FRAME_TAIL
 *命令数据：第一位：开启关闭电源，第二位：是否测量功耗，功耗数据2个字节，测量电流，第三个功耗模式，0正常功耗，1低功耗，第四个字节停止测试，不停止的话，就会在测试过程检测功耗等数据
 * 返回数据格式：第一位电源状态，第2，3字节，功耗数据，第四字节，功耗单位，0
 *mA,1 uA, 第5字节，功耗模式，第6字节，测试状态，0进行中，1完成，2失败
 * @param data 帧数据
 * @param len  帧长度
 */
static void handle_ft_control(const uint8_t *data, uint16_t len) {
  /*===========================================================================
   * 夹具控制帧字段定义 (FT_CONTROL)
   * 帧总长度: 36字节，有效载荷: 30字节 (data[4] ~ data[33])
   *===========================================================================*/

  // 验证最小帧长度
  if (len < 36) {
    log_e("FT控制帧长度错误: %d < 36", len);
    return;
  }

  uint8_t station = data[3];
  uint8_t local_station = PC_Protocol_GetStationId();

  // 工位匹配检查
  if (station != local_station) {
    log_d("工位不匹配: 收到%d, 本机%d", station, local_station);
    return;
  }

  log_i("收到夹具控制命令");
  // 标志位
  uint8_t control_status = 0; // 0是没有进入控制模式，1是进入控制模式
  control_status = data[4];
  /*---------------------------------------------------------------------------
   * 功能1: 主电源控制
   *---------------------------------------------------------------------------*/
  uint8_t main_power = data[5]; // 0=关闭, 1=开启, 0xFF=不操作

  /*---------------------------------------------------------------------------
   * 功能2: 从电源控制
   *---------------------------------------------------------------------------*/
  uint8_t aux_power = data[6]; // 0=关闭, 1=开启, 0xFF=不操作

  /*---------------------------------------------------------------------------
   * 功能3: 静态功耗测试
   *---------------------------------------------------------------------------*/
  uint8_t pwr_test_en = data[7]; // 0=停止, 1=开始(正常模式), 2=开始(低功耗模式)
  uint16_t pwr_interval = data[8] | (data[9] << 8); // 功耗采集间隔ms (16位)
  uint8_t pwr_avg_cnt = data[10];   // 每N次采集计算一次平均值，1=峰值模式
  uint8_t pwr_print_int = data[11]; // 功耗打印间隔秒，0=计算完立即打印
  uint8_t pwr_print_cnt = data[12]; // 打印N次后停止，0=持续打印

  /*---------------------------------------------------------------------------
   * 功能4: 阀门电压采集
   *---------------------------------------------------------------------------*/
  uint8_t valve_en = data[13];                          // 0=停止, 1=开始
  uint16_t valve_interval = data[14] | (data[15] << 8); // 阀门采集间隔ms
  uint8_t valve_avg_cnt = data[16];                     // 每N次计算平均值
  uint8_t valve_print_int = data[17];                   // 阀门打印间隔秒
  uint8_t valve_print_cnt = data[18];                   // 0=持续

  /*---------------------------------------------------------------------------
   * 功能5: 各路电压显示
   *---------------------------------------------------------------------------*/
  uint8_t volt_en = data[19];                          // 0=停止, 1=开始
  uint16_t volt_interval = data[20] | (data[21] << 8); // 各路电压采集间隔ms
  uint8_t volt_avg_cnt = data[22];                     // 每N次计算平均值
  uint8_t volt_print_int = data[23];                   // 各路电压打印间隔秒
  uint8_t volt_print_cnt = data[24];                   // 0=持续

  /*---------------------------------------------------------------------------
   * 功能6: 到位信号1输出 (OPEN)
   *---------------------------------------------------------------------------*/
  uint8_t pos1_en = data[25];  // 0=关闭, 1=开启
  uint8_t pos1_dur = data[26]; // 持续时间秒，0=一直开启，最大255秒

  /*---------------------------------------------------------------------------
   * 功能7: 到位信号2输出 (CLOSE)
   *---------------------------------------------------------------------------*/
  uint8_t pos2_en = data[27];  // 0=关闭, 1=开启
  uint8_t pos2_dur = data[28]; // 持续时间秒，0=一直开启，最大255秒

  /*---------------------------------------------------------------------------
   * 功能8: 霍尔1/电磁铁1控制
   *---------------------------------------------------------------------------*/
  uint8_t hall1_en = data[29];  // 0=关闭, 1=开启
  uint8_t hall1_dur = data[30]; // 持续时间秒，最大15秒(硬件限制，防烧毁)

  /*---------------------------------------------------------------------------
   * 功能9: 霍尔2/电磁铁2控制
   *---------------------------------------------------------------------------*/
  uint8_t hall2_en = data[31];  // 0=关闭, 1=开启
  uint8_t hall2_dur = data[32]; // 持续时间秒，最大15秒

  /*---------------------------------------------------------------------------
   * 功能10: 霍尔3/电磁铁3/开盖信号控制
   *---------------------------------------------------------------------------*/
  uint8_t hall3_en = data[33];  // 0=关闭, 1=开启
  uint8_t hall3_dur = data[34]; // 持续时间秒，最大15秒

  // 打印控制命令信息
  log_i("电源控制: 主电=%s, 从电=%s",
        main_power == 0xFF ? "不操作" : (main_power ? "开" : "关"),
        aux_power == 0xFF ? "不操作" : (aux_power ? "开" : "关"));

  log_i("功耗测试: %s, 间隔=%dms, 平均=%d次, 打印间隔=%ds, 打印次数=%d",
        pwr_test_en == 0 ? "停止"
                         : (pwr_test_en == 1 ? "正常模式" : "低功耗模式"),
        pwr_interval, pwr_avg_cnt, pwr_print_int, pwr_print_cnt);

  log_i("阀门电压: %s, 间隔=%dms, 平均=%d次, 打印间隔=%ds, 打印次数=%d",
        valve_en ? "开始" : "停止", valve_interval, valve_avg_cnt,
        valve_print_int, valve_print_cnt);

  log_i("各路电压: %s, 间隔=%dms, 平均=%d次, 打印间隔=%ds, 打印次数=%d",
        volt_en ? "开始" : "停止", volt_interval, volt_avg_cnt, volt_print_int,
        volt_print_cnt);

  log_i("到位信号: 1=%s(%ds), 2=%s(%ds)", pos1_en ? "开" : "关", pos1_dur,
        pos2_en ? "开" : "关", pos2_dur);

  log_i("霍尔控制: 1=%s(%ds), 2=%s(%ds), 3=%s(%ds)", hall1_en ? "开" : "关",
        hall1_dur, hall2_en ? "开" : "关", hall2_dur, hall3_en ? "开" : "关",
        hall3_dur);

  // 发送响应帧
  uint16_t pos = 0;
  s_tx_buffer[pos++] = FT_FRAME_HEAD;
  s_tx_buffer[pos++] = PC_CMD_FT_CONTROL_ACK; // 0xC3
  s_tx_buffer[pos++] =
      control_status; // 当前的控制状态，只有是1的时候，才表示进入控制模式，是0的时候退出控制模式
  s_tx_buffer[pos++] = 8; // 长度
  s_tx_buffer[pos++] = PC_Protocol_GetStationId();
  // 然后返回各帧的状态，实际就是和我们的控制命令一样
  s_tx_buffer[pos++] = main_power;
  s_tx_buffer[pos++] = aux_power;
  s_tx_buffer[pos++] = pwr_test_en;
  s_tx_buffer[pos++] = pwr_interval & 0xFF;
  s_tx_buffer[pos++] = (pwr_interval >> 8) & 0xFF;
  s_tx_buffer[pos++] = pwr_avg_cnt;
  s_tx_buffer[pos++] = pwr_print_int;
  s_tx_buffer[pos++] = pwr_print_cnt;
  s_tx_buffer[pos++] = valve_en;
  s_tx_buffer[pos++] = valve_interval & 0xFF;
  s_tx_buffer[pos++] = (valve_interval >> 8) & 0xFF;
  s_tx_buffer[pos++] = valve_avg_cnt;
  s_tx_buffer[pos++] = valve_print_int;
  s_tx_buffer[pos++] = valve_print_cnt;
  s_tx_buffer[pos++] = volt_en;
  s_tx_buffer[pos++] = volt_interval & 0xFF;
  s_tx_buffer[pos++] = (volt_interval >> 8) & 0xFF;
  s_tx_buffer[pos++] = volt_avg_cnt;
  s_tx_buffer[pos++] = volt_print_int;
  s_tx_buffer[pos++] = volt_print_cnt;
  s_tx_buffer[pos++] = pos1_en;
  s_tx_buffer[pos++] = pos1_dur;
  s_tx_buffer[pos++] = pos2_en;
  s_tx_buffer[pos++] = pos2_dur;
  s_tx_buffer[pos++] = hall1_en;
  s_tx_buffer[pos++] = hall1_dur;
  s_tx_buffer[pos++] = hall2_en;
  s_tx_buffer[pos++] = hall2_dur;
  s_tx_buffer[pos++] = hall3_en;
  s_tx_buffer[pos++] = hall3_dur;
  // 计算校验和
  uint8_t checksum = 0;
  for (uint16_t i = 0; i < pos; i++) {
    checksum += s_tx_buffer[i];
  }
  s_tx_buffer[pos++] = checksum;

  s_tx_buffer[pos++] = FT_FRAME_TAIL;

  log_d("发送工装Debug控制响应, 长度=%d", pos);

  // 发送响应
  if (s_send_func != NULL) {
    s_send_func(s_tx_buffer, pos);
  }

  // 通过回调调用实际的硬件控制函数
  PCFTControlFunc ft_control_func = PC_Protocol_GetFTControlFunc();
  if (ft_control_func != NULL) {
    ft_control_func(data, len);
  } else {
    log_w("工装控制回调未设置");
  }

  // 触发事件回调，通知应用层处理具体控制逻辑
  if (s_event_callback != NULL) {
    s_event_callback(PROTOCOL_EVENT_RECEIVED, PC_CMD_FT_CONTROL, data, len);
  }
}

/**
 * @brief 处理查询当前测试步骤命令 (0xBE)
 *
 * 协议格式:
 *   请求: 68 BE 06 [工位号] [校验和] 16
 *   响应: 68 BF [长度] [工位号] [测试状态] [失败原因] [步骤ID] [名称长度]
 * [步骤名称...] [原因长度] [原因名称...] [校验和] 16
 *
 * 功能: 查询当前测试状态、失败原因和失败步骤
 *
 * @param data 帧数据
 * @param len  帧长度
 */
static void handle_query_fail_step(const uint8_t *data, uint16_t len) {
  // 验证最小帧长度
  if (len < 6) {
    log_e("查询步骤帧长度错误: %d < 6", len);
    return;
  }

  uint8_t station = data[3];
  uint8_t local_station = PC_Protocol_GetStationId();

  // 工位匹配检查
  if (station != local_station) {
    log_d("工位不匹配: 收到%d, 本机%d", station, local_station);
    return;
  }

  // 验证校验和
  uint8_t calc_sum = 0;
  for (uint16_t i = 0; i < len - 2; i++) {
    calc_sum += data[i];
  }
  if (calc_sum != data[len - 2]) {
    log_e("查询步骤校验和错误");
    return;
  }

  // 获取当前步骤信息
  uint8_t step_id = 0;
  char step_name[64] = {0};
  uint8_t fail_reason = 0;
  char reason_name[64] = {0};
  uint8_t test_status =
      PC_Protocol_GetFailInfo(&step_id, step_name, &fail_reason, reason_name);

  log_i("测试状态: %s, 步骤: [%d] %s, 原因: [%d] %s",
        test_status == 0 ? "进行中" : (test_status == 1 ? "成功" : "失败"),
        step_id, step_name, fail_reason, reason_name);

  // 非Debug模式发送协议响应
  if (Debug_Mode == 0) {
    send_fail_step_response();
  }
}

/*============ 响应发送实现 ============*/

/**
 * @brief 发送配置应答 (0xAF)
 */
static void send_config_ack(void) {
  ConfigAckFrame frame = {.head = FT_FRAME_HEAD,
                          .cmd = PC_CMD_SET_CONFIG_ACK,
                          .length = 9,
                          .station_id = PC_Protocol_GetStationId(),
                          .debug_mode = Debug_Mode,
                          .passthrough_mode = PassThrough_Mode,
                          .preamble_mode = PassThrough_Preamble,
                          .checksum = 0,
                          .tail = FT_FRAME_TAIL};

  // 计算校验和 (前7个字节)
  uint8_t *p = (uint8_t *)&frame;
  for (int i = 0; i < 7; i++) {
    frame.checksum += p[i];
  }

  log_d("发送配置应答: 调试=%d, 透传=%d, 前导=%d", Debug_Mode, PassThrough_Mode,
        PassThrough_Preamble);

  if (s_send_func != NULL && Debug_Mode == 0) {
    s_send_func((uint8_t *)&frame, sizeof(frame));
  }
}

/**
 * @brief 发送当前测试步骤响应 (0xBF)
 *
 * 响应格式: 68 BF [长度] [工位号] [测试状态] [失败原因] [步骤ID] [名称长度]
 * [步骤名称...] [原因长度] [原因名称...] [校验和] 16
 */
static void send_fail_step_response(void) {
  uint16_t pos = 0;
  uint8_t checksum = 0;

  // 获取当前步骤信息
  uint8_t step_id = 0;
  char step_name[64] = {0};
  uint8_t fail_reason = 0;
  char reason_name[64] = {0};
  uint8_t test_status =
      PC_Protocol_GetFailInfo(&step_id, step_name, &fail_reason, reason_name);

  // 构建响应帧
  s_tx_buffer[pos++] = FT_FRAME_HEAD;
  s_tx_buffer[pos++] = PC_CMD_QUERY_FAIL_STEP_ACK; // 0xBF
  s_tx_buffer[pos++] = 0;                          // 长度占位

  // 工位号
  s_tx_buffer[pos++] = PC_Protocol_GetStationId();

  // 测试状态: 0=进行中, 1=成功, 2=失败
  s_tx_buffer[pos++] = test_status;

  // 失败原因代码
  s_tx_buffer[pos++] = fail_reason;

  // 当前步骤ID
  s_tx_buffer[pos++] = step_id;

  // 步骤名称 (带长度前缀的ASCII字符串)
  uint8_t name_len = strlen(step_name);
  if (name_len > 63)
    name_len = 63;
  s_tx_buffer[pos++] = name_len;
  memcpy(&s_tx_buffer[pos], step_name, name_len);
  pos += name_len;

  // 失败原因名称 (带长度前缀的ASCII字符串)
  uint8_t reason_len = strlen(reason_name);
  if (reason_len > 63)
    reason_len = 63;
  s_tx_buffer[pos++] = reason_len;
  memcpy(&s_tx_buffer[pos], reason_name, reason_len);
  pos += reason_len;

  // 设置长度
  s_tx_buffer[2] = pos + 2; // 加上校验和和帧尾

  // 计算校验和
  checksum = 0;
  for (uint16_t i = 0; i < pos; i++) {
    checksum += s_tx_buffer[i];
  }
  s_tx_buffer[pos++] = checksum;
  s_tx_buffer[pos++] = FT_FRAME_TAIL;

  log_d("发送步骤响应: 状态=%d, 原因=[%d]%s, 步骤=[%d]%s", test_status,
        fail_reason, reason_name, step_id, step_name);

  if (s_send_func != NULL) {
    s_send_func(s_tx_buffer, pos);
  }
}

/*============ 公共API ============*/

/**
 * @brief 获取当前调试模式状态
 * @return 0=关闭, 1=开启
 */
uint8_t PC_Config_GetDebugMode(void) { return Debug_Mode; }

/**
 * @brief 获取当前透传模式状态
 * @return 0=关闭, 1=开启
 */
uint8_t PC_Config_GetPassThroughMode(void) { return PassThrough_Mode; }

/**
 * @brief 设置调试模式
 * @param mode 0=关闭, 1=开启
 */
void PC_Config_SetDebugMode(uint8_t mode) {
  Debug_Mode = (mode != 0) ? 1 : 0;
  log_i("调试模式已设置为: %s", Debug_Mode ? "开" : "关");
}

/**
 * @brief 设置透传模式
 * @param mode 0=关闭, 1=开启
 */
void PC_Config_SetPassThroughMode(uint8_t mode) {
  PassThrough_Mode = (mode != 0) ? 1 : 0;
  log_i("透传模式已设置为: %s", PassThrough_Mode ? "开" : "关");
}
