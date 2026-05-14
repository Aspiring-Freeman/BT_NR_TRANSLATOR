/**
 * @file pc_protocol_diaphragm_gas_meter.c
 * @brief 国内膜式燃气表MES系统上位机通信协议实现
 * @version 2.0.0
 * @date 2025-01-20
 *
 * @section intro 简介
 * 实现与MES系统的通信协议（上位机协议）。
 * 协议格式: 68 [表号6字节] 68 [控制码] [长度2字节] [时间6字节] [设备类型]
 *           [数据标识2字节] [帧序号] [数据域...] [校验和] 16
 *
 * @section protocol_reference 协议参考 (PIC工程)
 * - 控制码: OPT_READ=0x01, OPT_WRITE=0x04, OPT_INSTALL=0x05
 * - 数据标识: DEV_START_TEST=0xFC03, DEV_GETCHECK_RESULT=0xFC04
 *
 * @section frame_index 帧索引定义
 * - Index_68Frame1=0, Index_MeterID=1, Index_68Frame2=7, Index_ControlCode=8
 * - Index_DataLength=9, Index_Time=11, Index_DiviceType=17, Index_DataMark=18
 * - Index_FrameSequence=20, Index_VolumeData=21
 */

#define LOG_TAG "pc_diaphragm_gas_meter"

#include "pc_protocol.h"
#include "utility.h"
#include <elog.h>
#include <stdio.h>
#include <string.h>

// 外部依赖
#include "main.h"
extern void test_start(void);
extern void protocol_debug_print(const uint8_t *data, uint16_t len);

/*============ 协议帧索引定义 (与PIC一致) ============*/

#define INDEX_68FRAME1 0     // 第一个68的起始索引
#define INDEX_METER_ID 1     // 表地址起始索引（6字节）
#define INDEX_68FRAME2 7     // 第二个68起始索引
#define INDEX_CONTROL_CODE 8 // 控制码索引
#define INDEX_DATA_LENGTH 9  // 数据长度起始索引（2字节，小端）
#define INDEX_TIME 11        // 时间的起始索引（6字节）
#define INDEX_DEVICE_TYPE 17 // 设备类型索引
#define INDEX_DATA_MARK 18   // 数据标识索引（2字节，小端）
#define INDEX_FRAME_SEQUENCE 20 // 帧序号索引
#define INDEX_VOLUME_DATA 21    // 数据域起始索引

#define DATA_CMD_LENGTH_FRONT 10 // 数据区前面固定长度

/*============ 控制码定义 (与PIC一致) ============*/

#define OPT_READ 0x01     // 读操作
#define OPT_WRITE 0x04    // 写操作
#define OPT_INSTALL 0x05  // 安装/配置操作
#define OPT_LOADLINK 0x08 // 加载链接

/*============ 数据标识定义 (与PIC一致) ============*/

#define DEV_TIME 0xC621            // 时间
#define DEV_START_TEST 0xFC03      // 启动测试
#define DEV_GETCHECK_RESULT 0xFC04 // 查询测试结果
#define DEV_AutoCheckFinish 0x1000 // 自检完成

/*============ 设备类型定义 ============*/

#define DEVICE_TYPE 0x08 // 设备类型

/*============ 内部变量 ============*/

static ProtocolSendFunc s_send_func = NULL;
static ProtocolEventCallback s_event_callback = NULL;

// 发送缓冲区
#define PC_TX_BUF_SIZE 256
static uint8_t s_tx_buffer[PC_TX_BUF_SIZE];

// RTC时间结构 (用于协议帧中的时间字段)
static uint8_t s_rtc_time[6] = {0x25, 0x01, 0x20,
                                0x10, 0x30, 0x00}; // 年月日时分秒

// 表号
static uint8_t s_meter_number[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};

/*============ 测试结果结构体 (与上位机协议对应) ============*/

// 检测结果结构体 (与 DiaphragmTestResult_t 一致)
#pragma pack(1)
typedef struct {
  uint8_t DeviceID;              // 工位ID
  uint8_t MeterTYP;              // 表具类型
  uint8_t IsOrNoWithIterm;       // 是否带附件
  uint8_t MasterVoult;           // 主电电压 (0.1V单位)
  uint8_t MasterLowPowerCurrent; // 主电静态功耗 (uA)
  uint8_t Module_Csq;            // 模块信号强度 CSQ
  uint8_t RTC_Volt;         // RTC电压 (0.1V单位), 实际为备电电压
  uint16_t FirmwareVersion; // 软件版本号
  uint8_t Reserve1;         // 保留1, 设置为0xF
  uint8_t
      IOStatus1; // IO状态1: 高4位=IC卡/119/阀门/计量, 低4位=EF/SIM卡/连接/模块
  uint8_t IOStatus2; // IO状态2: 高4位=保留/蓝牙/倾斜, 低4位=开盖/温压/红外/RTC
  uint8_t ModuleIMEI[15];       // 模块IMEI, ASCII格式, 固定15字节
  uint8_t ModuleIMSI[15];       // 模块IMSI, ASCII格式, 固定15字节
  uint8_t ModuleICCID[20];      // 模块ICCID, ASCII格式, 固定20字节
  uint8_t ModulePowerStatus;    // 模块备电状态, 0=正常, 1=异常
  uint8_t FirmwareBuildTime[6]; // 软件版本编译时间
  uint8_t StarMac[12];          // 星闪MAC地址
  uint8_t ESAMID[8];            // ESAM ID
  uint8_t PressureOnBoard[4];   // 板载压力值
} GasCheckResult_S;
#pragma pack()

// 当前检测结果
static GasCheckResult_S s_check_result;

/*============ 内部函数声明 ============*/

static bool mes_init(void);
static ProtocolResult mes_parse(uint8_t *data, uint16_t len);
static bool mes_send_cmd(uint16_t cmd, void *param);
static void mes_on_response(uint16_t code, const uint8_t *data, uint16_t len);
static void mes_set_send_func(ProtocolSendFunc func);
static void mes_set_event_callback(ProtocolEventCallback callback);

// 命令处理函数
static void handle_start_test(const uint8_t *data, uint16_t len);
static void handle_query_result(const uint8_t *data, uint16_t len);
static void handle_set_config(const uint8_t *data, uint16_t len);

// 响应发送函数
static uint16_t build_response_frame(uint8_t *buf, uint8_t ctrl_code,
                                     uint16_t data_mark, const uint8_t *data,
                                     uint16_t data_len);
static void send_start_test_ack(void);
static void send_test_result(void);
static void send_config_ack(void);

/*============ 协议接口实例 ============*/

const ProtocolInterface diaphragm_gas_meter_pc_protocol = {
    .name = "diaphragm_gas_meter",
    .init = mes_init,
    .parse = mes_parse,
    .send_cmd = mes_send_cmd,
    .on_response = mes_on_response,
    .set_send_func = mes_set_send_func,
    .set_event_callback = mes_set_event_callback,
};

/*============ 接口实现 ============*/

static bool mes_init(void) {
  log_i("国内膜式燃气表MES协议初始化");
  // 初始化检测结果结构体
  memset(&s_check_result, 0, sizeof(GasCheckResult_S));
  s_check_result.DeviceID = PC_Protocol_GetStationId();
  s_check_result.Reserve1 = 0xFF;       // 保留字段设置为0xFF
  s_check_result.ModulePowerStatus = 1; // 默认备电异常
  log_i("本机工位号: %d", s_check_result.DeviceID);
  return true;
}

/**
 * @brief 解析上位机协议
 *
 * 协议帧格式:
 * [0]    68          - 帧头1
 * [1-6]  表号        - 6字节
 * [7]    68          - 帧头2
 * [8]    控制码      - OPT_READ/OPT_WRITE/OPT_INSTALL
 * [9-10] 数据域长度  - 2字节小端
 * [11-16] 时间       - 6字节BCD
 * [17]   设备类型    - 1字节
 * [18-19] 数据标识   - 2字节小端
 * [20]   帧序号      - 1字节
 * [21...] 数据域     - 可变长度
 * [n-1]  校验和      - 1字节
 * [n]    16          - 帧尾
 */
static ProtocolResult mes_parse(uint8_t *data, uint16_t len) {
  uint16_t pos = 0;
  bool handled = false;

  log_d("PC协议开始解析, 长度=%d", len);
  elog_hexdump("PC_RX", 8, data, len);

  // 最小帧长度: 68 + 表号(6) + 68 + 控制码 + 长度(2) + 时间(6) + 设备类型 +
  // 数据标识(2) + 帧序号 + 校验和 + 16 = 23字节
  while (pos + 23 <= len) {
    // 查找第一个帧头
    if (data[pos + INDEX_68FRAME1] != FRAME_HEAD_68) {
      pos++;
      continue;
    }

    // 验证第二个帧头
    if (data[pos + INDEX_68FRAME2] != FRAME_HEAD_68) {
      pos++;
      continue;
    }

    // 获取数据域长度 (小端模式)
    uint16_t data_field_len = READ_LE_U16(&data[pos + INDEX_DATA_LENGTH]);

    // 计算完整帧长度: 固定头(11) + 数据域长度 + 校验和(1) + 帧尾(1)
    // 注意: PIC中数据域长度包含了时间到数据结束的部分
    uint16_t frame_len = 11 + data_field_len + 2;

    // 验证长度
    if (pos + frame_len > len) {
      log_d("帧不完整, 需要%d字节, 当前只有%d字节", frame_len, len - pos);
      return PROTOCOL_RESULT_INCOMPLETE;
    }

    // 验证帧尾
    if (data[pos + frame_len - 1] != FRAME_TAIL_16) {
      log_d("帧尾错误: 0x%02X", data[pos + frame_len - 1]);
      pos++;
      continue;
    }

    // 验证校验和
    uint8_t recv_checksum = data[pos + frame_len - 2];
    uint8_t calc_checksum_val = util_checksum_sum8(&data[pos], frame_len - 2);
    if (recv_checksum != calc_checksum_val) {
      log_e("校验和错误: 计算=0x%02X, 接收=0x%02X", calc_checksum_val,
            recv_checksum);
      pos++;
      continue;
    }

    // 获取控制码和数据标识
    uint8_t ctrl_code = data[pos + INDEX_CONTROL_CODE];
    uint16_t data_mark = READ_LE_U16(&data[pos + INDEX_DATA_MARK]);

    // 获取工位号 (数据域第一个字节)
    uint8_t station_id = data[pos + INDEX_VOLUME_DATA];
    uint8_t local_station = PC_Protocol_GetStationId();

    // 工位匹配检查
    if (station_id != local_station) {
      log_d("工位不匹配: 命令=%d, 本机=%d", station_id, local_station);
      pos += frame_len;
      continue;
    }

    // 保存表号
    memcpy(s_meter_number, &data[pos + INDEX_METER_ID], 6);

    // 保存时间
    memcpy(s_rtc_time, &data[pos + INDEX_TIME], 6);

    // 根据控制码和数据标识分发处理
    switch (ctrl_code) {
    case OPT_INSTALL: // 0x05 - 安装/启动命令
      if (data_mark == DEV_START_TEST) {
        log_d("收到启动测试命令 (0xFC03)");
        handle_start_test(&data[pos], frame_len);
        handled = true;
      }
      break;

    case OPT_READ: // 0x01 - 读操作
      if (data_mark == DEV_GETCHECK_RESULT) {
        log_d("收到查询结果命令 (0xFC04)");
        handle_query_result(&data[pos], frame_len);
        handled = true;
      }
      break;

    case OPT_WRITE: // 0x04 - 写操作
      if (data_mark == DEV_TIME) {
        log_d("收到设置时间命令 (0xC621)");
        // 处理时间设置
        handled = true;
      }
      break;

    default:
      log_d("未处理的控制码: 0x%02X", ctrl_code);
      break;
    }

    // 私有扩展命令 (非标准MES协议)
    if (!handled && ctrl_code == PC_CMD_SET_CONFIG) {
      handle_set_config(&data[pos], frame_len);
      handled = true;
    }

    pos += frame_len;
  }

  return handled ? PROTOCOL_RESULT_OK : PROTOCOL_RESULT_UNKNOWN_CMD;
}

static bool mes_send_cmd(uint16_t cmd, void *param) {
  switch (cmd) {
  case PC_CMD_START_TEST_ACK:
    send_start_test_ack();
    return true;

  case PC_CMD_RESULT_RESPONSE:
    send_test_result();
    return true;

  case PC_CMD_SET_CONFIG_ACK:
    send_config_ack();
    return true;

  default:
    log_w("未支持的发送命令: 0x%04X", cmd);
    return false;
  }
}

static void mes_on_response(uint16_t code, const uint8_t *data, uint16_t len) {
  log_d("PC协议收到响应: 0x%04X", code);
}

static void mes_set_send_func(ProtocolSendFunc func) { s_send_func = func; }

static void mes_set_event_callback(ProtocolEventCallback callback) {
  s_event_callback = callback;
}

/*============ 命令处理实现 ============*/

/**
 * @brief 处理启动测试命令 (OPT_INSTALL + DEV_START_TEST)
 */
static void handle_start_test(const uint8_t *data, uint16_t len) {
  log_i("处理启动测试命令");

  // 获取工位号
  uint8_t station_id = data[INDEX_VOLUME_DATA];
  s_check_result.DeviceID = station_id;

  // 获取表号 (数据域第2-8字节)
  // 注意: PIC中表号在数据域内，跟在工位号后面
  if (len > INDEX_VOLUME_DATA + 7) {
    // memcpy(Meter.Num, &data[INDEX_VOLUME_DATA + 1], 7);
    log_d("表号已保存");
  }

  // 清空检测结果
  memset(&s_check_result.MeterTYP, 0xFF, sizeof(GasCheckResult_S) - 1);
  s_check_result.DeviceID = station_id;

  // 启动测试
  log_i("启动测试...");
  test_start();

  // 发送应答
  send_start_test_ack();
}

/**
 * @brief 处理查询结果命令 (OPT_READ + DEV_GETCHECK_RESULT)
 */
static void handle_query_result(const uint8_t *data, uint16_t len) {
  log_i("处理查询结果命令");

  // 检查测试是否已结束:
  // 1. 当前步骤已到达END步骤 (测试刚结束，on_enter还在执行)
  // 2. 或者test_completed标志已设置 (测试已完成，步骤可能已重置)
  if (diaphragm_test_step != DIAPHRAGM_END &&
      !diaphragm_runtime.test_completed) {
    log_d("测试未结束，不响应 (当前状态=%d)", diaphragm_test_step);
    return; // 测试未结束，不发送响应
  }

  // 发送测试结果
  send_test_result();
}

/**
 * @brief 处理配置命令 (私有扩展)
 */
static void handle_set_config(const uint8_t *data, uint16_t len) {
  if (len < 8) {
    log_e("配置帧长度错误");
    return;
  }

  // 设置调试模式
  Debug_Mode = (data[4] != 0) ? 1 : 0;
  log_d("调试模式: %s", Debug_Mode ? "开" : "关");

  // 设置透传模式
  PassThrough_Mode = (data[5] != 0) ? 1 : 0;
  log_d("透传模式: %s", PassThrough_Mode ? "开" : "关");

  // 发送应答
  send_config_ack();
}

/*============ 响应发送实现 ============*/

/**
 * @brief 构建响应帧
 *
 * @param buf 输出缓冲区
 * @param ctrl_code 控制码 (带0x80应答标志)
 * @param data_mark 数据标识
 * @param data 数据域内容
 * @param data_len 数据域长度
 * @return 帧总长度
 */
static uint16_t build_response_frame(uint8_t *buf, uint8_t ctrl_code,
                                     uint16_t data_mark, const uint8_t *data,
                                     uint16_t data_len) {
  uint16_t pos = 0;

  // 帧头1
  buf[pos++] = FRAME_HEAD_68;

  // 表号 (6字节)
  memcpy(&buf[pos], s_meter_number, 6);
  pos += 6;

  // 帧头2
  buf[pos++] = FRAME_HEAD_68;

  // 控制码 (带0x80应答标志)
  buf[pos++] = ctrl_code | 0x80;

  // 数据域长度 (2字节小端) - 先占位，后面填充
  uint16_t len_pos = pos;
  pos += 2;

  // 时间 (6字节)
  memcpy(&buf[pos], s_rtc_time, 6);
  pos += 6;

  // 设备类型
  buf[pos++] = DEVICE_TYPE;

  // 数据标识 (2字节小端)
  WRITE_LE_U16(&buf[pos], data_mark);
  pos += 2;

  // 帧序号
  buf[pos++] = 0;

  // 数据域
  if (data != NULL && data_len > 0) {
    memcpy(&buf[pos], data, data_len);
    pos += data_len;
  }

  // 填充数据域长度 (从时间开始到数据结束)
  uint16_t data_field_len =
      pos - 11; // pos - (帧头1 + 表号 + 帧头2 + 控制码 + 长度字段) = pos - 11
  WRITE_LE_U16(&buf[len_pos], data_field_len);

  // 校验和
  buf[pos] = util_checksum_sum8(buf, pos);
  pos++;

  // 帧尾
  buf[pos++] = FRAME_TAIL_16;

  return pos;
}

/**
 * @brief 发送启动测试应答
 */
static void send_start_test_ack(void) {
  uint8_t data[8];
  uint16_t data_len = 0;

  // 工位号
  data[data_len++] = PC_Protocol_GetStationId();

  // 表号 (7字节，与PIC一致)
  memcpy(&data[data_len], s_meter_number, 6);
  data_len += 6;
  data[data_len++] = 0; // 第7字节

  uint16_t frame_len = build_response_frame(s_tx_buffer, OPT_INSTALL,
                                            DEV_START_TEST, data, data_len);

  log_d("发送启动测试应答, 长度=%d", frame_len);
  elog_hexdump("PC_TX", 8, s_tx_buffer, frame_len);

  if (s_send_func != NULL && Debug_Mode == 0) {
    s_send_func(s_tx_buffer, frame_len);
  } else {
    log_i("当前的响应帧的实际内容是：");
    elog_hexdump("PC_TX", 8, s_tx_buffer, frame_len);
  }
}

/**
 * @brief 发送测试结果
 *
 * 与上位机协议对应，返回GasCheckResult_S结构体
 */
static void send_test_result(void) {
  // 从 diaphragm_test_result 获取测试结果，填充到 s_check_result
  s_check_result.DeviceID = PC_Protocol_GetStationId();

  // 填充测试结果 (从 diaphragm_test_result 获取)
  // 注意：无论测试是否结束，都应该返回当前已收集的结果（边测试边填充）
  {
    // 表具类型和附件信息
    s_check_result.MeterTYP = diaphragm_test_result.MeterTYP; // 0 霍尔，1光电
    s_check_result.IsOrNoWithIterm =
        diaphragm_test_result
            .IsOrNoWithIterm; // 是否带附件,共8位，实际就是当前1001的附件状态，测试流程中获得1001直接赋值过来了，只有低8位有效

    // 电压 (已经是0.1V单位)
    s_check_result.MasterVoult =
        diaphragm_test_result.MasterVoult; // 十进制 65/10=6.5 供电电压

    // 功耗
    s_check_result.MasterLowPowerCurrent =
        diaphragm_test_result.MasterLowPowerCurrent; // 静态电流：十进制41

    // 信号强度
    s_check_result.Module_Csq = diaphragm_test_result.Module_Csq;
    log_i("模块信号强度 CSQ: %d", s_check_result.Module_Csq);
    // RTC电压
    s_check_result.RTC_Volt =
        diaphragm_test_result
            .RTC_Volt; // RTC电压，这里是协议读取的，时钟电压：十进制
                       // 48 / 10 = 4.8

    // 版本号，固件版本
    s_check_result.FirmwareVersion = diaphragm_test_result.FirmwareVersion;

    // 保留字段
    s_check_result.Reserve1 = 0xFF;

    // IO状态位
    s_check_result.IOStatus1 =
        diaphragm_test_result
            .IOStatus1; // 示例，0xEF  1110依次代表 IC卡，119，阀门，计量状态
                        //  1111依次代表 EF,SIM卡,连接，模块状态
    s_check_result.IOStatus2 =
        diaphragm_test_result
            .IOStatus2; // 第三位示例：1111前两位1为保留，后两位11表示蓝牙状态和倾斜开关状态，
    // 1111依次代表开盖检测，温压，红外，RTC状态
    // 开盖状态，0为低电平，1为正常(高电平)
    // 蓝牙状态  0为异常，1为正常（仅校验串口通信）
    // 倾斜开关状态 0为正常，1为异常
    // 考虑到兼容性问题，这一部分最好做成可配置是否检测的

    // IMEI/IMSI/ICCID 返回的值是不包含结束符的
    memcpy(s_check_result.ModuleIMEI, diaphragm_test_result.ModuleIMEI, 15);
    memcpy(s_check_result.ModuleIMSI, diaphragm_test_result.ModuleIMSI, 15);
    memcpy(s_check_result.ModuleICCID, diaphragm_test_result.ModuleICCID, 20);

    // 备电状态
    s_check_result.ModulePowerStatus =
        diaphragm_test_result
            .ModulePowerStatus; // 无备电，赋值0x00 ,有备电赋值0x01
    s_check_result.ModulePowerStatus = 0x00;

    // 版本编译时间
    memcpy(s_check_result.FirmwareBuildTime,
           diaphragm_test_result.FirmwareBuildTime, 6);

    // 星闪MAC (12字节，不含结束符)
    memcpy(s_check_result.StarMac, diaphragm_test_result.StarMac, 12);

    // ESAM ID
    memcpy(s_check_result.ESAMID, diaphragm_test_result.ESAMID, 8);

    // 板载压力
    memcpy(s_check_result.PressureOnBoard,
           diaphragm_test_result.PressureOnBoard, 4);
  }

  // 构建响应帧
  uint16_t frame_len = build_response_frame(
      s_tx_buffer, OPT_READ, DEV_GETCHECK_RESULT, (uint8_t *)&s_check_result,
      sizeof(GasCheckResult_S));

  // 调试：打印关键字段值
  log_i("测试结果关键数据: CSQ=%d(src=%d), RTC=%d(src=%d), Ver=0x%04X",
        s_check_result.Module_Csq, diaphragm_test_result.Module_Csq,
        s_check_result.RTC_Volt, diaphragm_test_result.RTC_Volt,
        s_check_result.FirmwareVersion);

  log_d("发送测试结果, 长度=%d, 结构体大小=%d", frame_len,
        sizeof(GasCheckResult_S));
  elog_hexdump("PC_TX", 8, s_tx_buffer, frame_len);

  if (s_send_func != NULL && Debug_Mode == 0) {
    s_send_func(s_tx_buffer, frame_len);
  }
}

/**
 * @brief 发送配置应答
 */
static void send_config_ack(void) {
  uint8_t data[3];
  data[0] = PC_Protocol_GetStationId();
  data[1] = Debug_Mode;
  data[2] = PassThrough_Mode;

  uint16_t frame_len =
      build_response_frame(s_tx_buffer, PC_CMD_SET_CONFIG, 0x0000, data, 3);

  log_d("发送配置应答");

  if (s_send_func != NULL && Debug_Mode == 0) {
    s_send_func(s_tx_buffer, frame_len);
  }
}

/*============ 公共API实现 ============*/

/**
 * @brief 发送测试结果到PC端 (膜式燃气表版本)
 */
void PC_GasMeter_SendTestResult(void) { send_test_result(); }

/**
 * @brief 更新检测结果中的IO状态位
 * @param status_reg 状态寄存器: 1=IOStatus1, 2=IOStatus2
 * @param bit_pos 位位置 (0-7)
 * @param value 值 (0或1)
 */
void PC_UpdateCheckStatus(uint8_t status_reg, uint8_t bit_pos, uint8_t value) {
  if (bit_pos > 7)
    return;

  if (status_reg == 1) {
    if (value) {
      s_check_result.IOStatus1 |= (1 << bit_pos);
    } else {
      s_check_result.IOStatus1 &= ~(1 << bit_pos);
    }
  } else if (status_reg == 2) {
    if (value) {
      s_check_result.IOStatus2 |= (1 << bit_pos);
    } else {
      s_check_result.IOStatus2 &= ~(1 << bit_pos);
    }
  }
}

/**
 * @brief 调试模式下打印测试结果 (膜式燃气表版本)
 */
void PC_GasMeter_TestResultAnalysis(void) {
  if (Debug_Mode == 0) {
    return;
  }

  log_d("\r\n========================================");
  log_d("           膜式燃气表检测结果汇总");
  log_d("========================================");

  // 基本信息
  log_d("【基本信息】");
  log_d("  工位ID: %d", s_check_result.DeviceID);
  log_d("  表具类型: %d", s_check_result.MeterTYP);
  log_d("  附件信息: 0x%02X", s_check_result.IsOrNoWithIterm);
  log_d("----------------------------------------");

  // 电压检测
  log_d("【电压检测】");
  log_d("  主电电压: %d.%dV", s_check_result.MasterVoult / 10,
        s_check_result.MasterVoult % 10);
  log_d("  备电电压: %d.%dV", s_check_result.RTC_Volt / 10,
        s_check_result.RTC_Volt % 10);
  log_d("----------------------------------------");

  // 功耗检测
  log_d("【功耗检测】");
  log_d("  静态电流: %d uA", s_check_result.MasterLowPowerCurrent);
  log_d("----------------------------------------");

  // 通信检测
  log_d("【检测项状态】");
  log_d("  信号强度: %d", s_check_result.Module_Csq);
  log_d("  IOStatus1: 0x%02X", s_check_result.IOStatus1);
  log_d("    模块: %s", (s_check_result.IOStatus1 & 0x01) ? "正常" : "异常");
  log_d("    连接: %s", (s_check_result.IOStatus1 & 0x02) ? "正常" : "异常");
  log_d("    SIM卡: %s", (s_check_result.IOStatus1 & 0x04) ? "正常" : "异常");
  log_d("    EEPROM: %s", (s_check_result.IOStatus1 & 0x08) ? "正常" : "异常");
  log_d("    计量: %s", (s_check_result.IOStatus1 & 0x10) ? "正常" : "异常");
  log_d("    阀门: %s", (s_check_result.IOStatus1 & 0x20) ? "正常" : "异常");
  log_d("    119: %s", (s_check_result.IOStatus1 & 0x40) ? "正常" : "异常");
  log_d("    IC卡: %s", (s_check_result.IOStatus1 & 0x80) ? "正常" : "异常");
  log_d("  IOStatus2: 0x%02X", s_check_result.IOStatus2);
  log_d("    RTC: %s", (s_check_result.IOStatus2 & 0x01) ? "正常" : "异常");
  log_d("    红外: %s", (s_check_result.IOStatus2 & 0x02) ? "正常" : "异常");
  log_d("    温压: %s", (s_check_result.IOStatus2 & 0x04) ? "正常" : "异常");
  log_d("    开盖: %s", (s_check_result.IOStatus2 & 0x08) ? "正常" : "异常");
  log_d("    倾斜: %s", (s_check_result.IOStatus2 & 0x10) ? "正常" : "异常");
  log_d("    蓝牙: %s", (s_check_result.IOStatus2 & 0x20) ? "正常" : "异常");
  log_d("  备电状态: %s",
        s_check_result.ModulePowerStatus == 0 ? "正常" : "异常");
  log_d("----------------------------------------");

  // 模块信息
  log_d("【模块信息】");
  log_d("  IMEI: %s", s_check_result.ModuleIMEI);
  log_d("  IMSI: %s", s_check_result.ModuleIMSI);
  log_d("  ICCID: %s", s_check_result.ModuleICCID);
  log_d("  星闪MAC: %s", s_check_result.StarMac);
  log_d("  版本号: 0x%04X", s_check_result.FirmwareVersion);
  log_d("========================================\r\n");
}
