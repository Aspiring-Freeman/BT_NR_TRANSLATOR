/* EasyLogger LOG_TAG 必须在 elog.h 之前定义 */
#define LOG_TAG "main"

#include "main.h"
#include "app_config.h"

/* BSP */
#include "GPIO.h"
#include "GPTIM.h"
#include "Peripheral/uart/ble_uart.h"
#include "Peripheral/uart/pc_uart.h"

/* 组件库 */
#include "ble.h"
#include "components.h"
#include "elog_user_config.h"
// #include "time_manager.h"
#include <inttypes.h>
/* EasyLogger */
#include "elog.h"
#include "elog_user_config.h"

/* Protocol */
#include "protocol.h"
#include "protocol_manager.h"

/* 构建信息 (由 cmake/generate_build_info.cmake 自动生成) */
#include "LED_CTRL.h"
#include "build_info.h"
/*============================================================================
 *                              全局变量
 *============================================================================*/
uint8_t Debug_Mode = 1;       /**< 调试模式: 0=关闭, 1=开启 */
uint8_t PassThrough_Mode = 0; /**< 透传模式: 0=普通, 1=透传 */

uint8_t PassThrough_Preamble =
    0; /**< 透传前导: 0=无前导,
          1=有前导,默认状态无前导，避免哪天开了忘记关闭导致测试时间增加 */
/*============================================================================
 *                              外部声明
 *============================================================================*/
/* 测试相关 */
uint16_t debug_print_time = DEBUG_PRINT_TIME;
uint32_t protocol_response_param = 0;

/*============================================================================
 *  前向声明
 *===========================================================================*/
static void System_Init(void);
static void Peripheral_Init(void);
/* 回调函数 (供协议层调用) */
// 获取当前固件版本号
static uint16_t Callback_GetVersion(void);
static void Callback_GetBuildTime(char *build_time);
static void Callback_FTControl(const uint8_t *data, uint16_t length);

/* 主循环 */
static void Loop_PassThrough(void);
static void Loop_NormalTest(void);

/* 调试辅助 */
static void Print_SystemInfo(void);
// static void Board_SelfTest(void);

/*============================================================================
 *  主函数
 *===========================================================================*/
int main(void) {
  System_Init();
  Peripheral_Init();
  /* 显示系统信息 */
  Print_SystemInfo();

  log_i("BT_PassThroughFT v%d.%d.%d 启动", SOFTWARE_VERSION_MAJOR,
        SOFTWARE_VERSION_MINOR, SOFTWARE_VERSION_PATCH);
  log_i("进入主循环");

  /* 1. 将 ISR 接收缓冲迁移到公共缓冲（帧超时后触发）*/
  PCUart_Rx_Process();
  BLEUart_Rx_Process();

  if (PassThrough_Mode) {
    log_i("进入透传模式主循环");
    Loop_PassThrough();
  } else {
    log_i("进入正常测试模式主循环");
    Loop_NormalTest();
  }
}

/*============================================================================
 *  系统初始化（时钟 / Flash 等）
 *===========================================================================*/
static void System_Init(void) {
  FL_Init();
  MF_Clock_Init();
  MF_SystemClock_Config();
}

/*============================================================================
 *  外设初始化
 *===========================================================================*/
static void Peripheral_Init(void) {
  /* GPIO（LED 等）*/
  GPIO_Init();

  /* UART */
  PCUart_Init();
  BLEUart_Init();

  /* GPTIM0：1 ms 帧超时计时 */
  MF_GPTIM0_Config_Init();
  GPTIM0_Start();

  /* EasyLogger */
  ELog_UserInit();

  /* 协议框架 */
  ProtocolManager_Init();
  ProtocolManager_RegisterPC(&config_pc_protocol);
  ProtocolManager_SetActivePC("pc_config");
  ProtocolManager_SetPCSendFunc((ProtocolSendFunc)PCUart_Tx_Send);

  /* 组件系统初始化 */
  ComponentsConfig components_cfg = {
      .pc_send = PCUart_Tx_Send,
      .bluetooth_send = BLEUart_Tx_Send, // 蓝牙协议发送函数
      .get_version = Callback_GetVersion,
      .get_build_time = Callback_GetBuildTime,
      .ft_control = Callback_FTControl,
  };
  Components_Init(&components_cfg);

  /* 蓝牙应用层初始化（注册设备协议 + 事件回调，连接协议层与App层） */
  BLE_INIT();
}

// 工装辅助函数，方便调试
/**
 * @brief 获取程序版本号回调
 * @return 版本号 (高字节=主版本, 低字节=次版本)
 */
static uint16_t Callback_GetVersion(void) {
  return (SOFTWARE_VERSION_MAJOR << 8) | (SOFTWARE_VERSION_MINOR << 4) |
         SOFTWARE_VERSION_PATCH;
}

/**
 * @brief 获取编译时间回调
 * @param[out] build_time 编译时间字符串缓冲区 (至少32字节)
 * @note 追加 git 短 hash + dirty 标记, 例如 "Apr 21 2026 18:14:14 gabc1234+"
 */
static void Callback_GetBuildTime(char *build_time) {
  if (build_time != NULL) {
    snprintf(build_time, 32, "%s %s %s", __DATE__, __TIME__, BUILD_GIT_INFO);
  }
}

static void Callback_FTControl(const uint8_t *data, uint16_t length) {
  if (data == NULL || length == 0) {
    log_w("工装控制指令参数无效");
    return;
  }

  // 暂时屏蔽，需要修改
  //  diaphragm_board_debug_handle_command(data, length);
}

/**
 * @brief 打印系统信息
 */
static void Print_SystemInfo(void) {
  log_i("============================================");
  log_i("  NB_IOT_Gas_Meter_Board_18 %s", SOFTWARE_VERSION_STRING);
  log_i("  Build: %s %s", __DATE__, __TIME__);
  log_i("  Git  : branch=%s hash=%s%s", BUILD_GIT_BRANCH, BUILD_GIT_HASH,
        BUILD_GIT_DIRTY ? " (dirty)" : "");
  log_i("============================================");
}

/*@brief 透传模式主循环 *@note PC<--> 蓝牙 双向透传*/
static void Loop_PassThrough(void) {

  while (PassThrough_Mode) {
    /* 接收处理：将中断缓冲区数据拷贝到公有缓冲区 */
    PCUart_Rx_Process();
    BLEUart_Rx_Process();

    // PC->蓝牙
    if (pc_uart_rx_data_flag == 1 && pc_uart_rx_frame_timeout == 0) {
      /* 检查是否是控制指令 (68 AE ...) */
      if (pc_uart_rx_count >= 8 && pc_uart_rx_buffer[0] == FT_FRAME_HEAD) {
        // 如果是工装配置协议帧头，继续进行验证工装帧尾是否正确
        uint8_t frame_length = pc_uart_rx_buffer[2];
        if (pc_uart_rx_buffer[0 + frame_length - 1] == FT_FRAME_TAIL) {
          log_d("收到工装配置指令,即将关闭透传模式并进行协议解析");
          Protocol_PC_Parse(pc_uart_rx_buffer, pc_uart_rx_count);
          pc_uart_rx_data_flag = 0;
          pc_uart_rx_count = 0;
          continue;
        }
        Protocol_PC_Parse(pc_uart_rx_buffer, pc_uart_rx_count);
      } else {
        /* 普通数据转发 */
        log_d(">> PC->蓝牙:");
        protocol_debug_print(pc_uart_rx_buffer, pc_uart_rx_count);
        BLEUart_Tx_Send(pc_uart_rx_buffer, pc_uart_rx_count);
      }
      pc_uart_rx_data_flag = 0;
      pc_uart_rx_count = 0;
    }

    // 蓝牙 -> PC
    if (ble_uart_rx_data_flag == 1 && ble_uart_rx_frame_timeout == 0) {
      log_d(">> 蓝牙->PC:");
      protocol_debug_print(ble_uart_rx_buffer, ble_uart_rx_count);
      /* 先交由 DS809 + bt_app_proto 双解析器处理 (握手/查询/APP 应用层)
       * 未被识别的帧会被丢弃; 如需透传可同时调 PCUart_Tx_Send */
      BLE_OnRxData(ble_uart_rx_buffer, ble_uart_rx_count);
      ble_uart_rx_data_flag = 0;
      ble_uart_rx_count = 0;
    }

    BLE_Task(); /* 握手状态机 + 后台状态查询 */

#ifdef ENABLE_WATCHDOG
    FL_IWDT_ReloadCounter(IWDT);
#endif
  }

  log_i("退出透传模式");
}

static void Loop_NormalTest(void) {
  while (1) {
    /* 检测模式切换 */
    if (PassThrough_Mode) {
      log_i("检测到透传模式开启，切换到透传循环");
      Loop_PassThrough();
      log_i("透传模式关闭，返回正常测试模式");
      continue;
    }
    PCUart_Rx_Process();  /* PC调试 */
    BLEUart_Rx_Process(); /* 蓝牙调试 */
    LedCheck();

    /* 蓝牙上行数据 → DS809 + bt_app_proto 双协议并行解析 */
    if (ble_uart_rx_data_flag == 1 && ble_uart_rx_frame_timeout == 0) {
      BLE_OnRxData(ble_uart_rx_buffer, ble_uart_rx_count);
      ble_uart_rx_data_flag = 0;
      ble_uart_rx_count = 0;
    }

    BLE_Task(); /* 握手状态机 + 后台状态查询 */

    if (debug_print_time == 0) {
      debug_print_time = DEBUG_PRINT_TIME;
      log_i("正常测试模式运行中...");
    }
#ifdef ENABLE_WATCHDOG
    FL_IWDT_ReloadCounter(IWDT);
#endif
  }
}
