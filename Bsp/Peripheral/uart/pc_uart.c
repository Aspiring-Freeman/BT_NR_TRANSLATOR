/**
 * @file    pc_uart.c
 * @brief   PC 上位机 UART 纯硬件驱动 (UART0 / PA2-RX / PA3-TX / FM33LE0xx)
 *
 * 架构说明:
 *   - ISR 将数据写入私有 s_rx_buf[]，并重载 pc_uart_rx_frame_timeout
 *   - PCUart_Rx_Process() 在帧超时此将私有缓冲复制到公共 pc_uart_rx_buffer[]
 *   - 主循环检查 pc_uart_rx_data_flag 并调用协议解析
 */
#include "pc_uart.h"
#include "Protocol/protocol.h"
#include "fm33le0xx_fl.h"
#include "main.h"
#include <elog.h>
#include <string.h>

/*---------------------------------------------------------------------------
 *  私有 ISR 缓冲区
 *--------------------------------------------------------------------------*/
static uint8_t s_rx_buf[PC_UART_BUFFER_SIZE];
static volatile uint16_t s_rx_count = 0;
static volatile uint8_t s_rx_flag = 0;
static volatile uint8_t s_rx_overflow_flag = 0;

/*---------------------------------------------------------------------------
 *  公共接收缓冲区（主循环读取）
 *--------------------------------------------------------------------------*/
volatile uint8_t pc_uart_rx_data_flag = 0;
uint8_t pc_uart_rx_buffer[PC_UART_BUFFER_SIZE];
volatile uint16_t pc_uart_rx_count = 0;
volatile uint16_t pc_uart_rx_frame_timeout = 0; /* GPTIM0 每 ms 递减 */

/*---------------------------------------------------------------------------
 *  私有 TX 缓冲区
 *--------------------------------------------------------------------------*/
static uint8_t s_tx_buf[PC_UART_BUFFER_SIZE];
static uint16_t s_tx_total = 0;
static uint16_t s_tx_index = 0;

/* 接收处理缓冲区（供协议解析期间使用，避免与 ISR 缓冲区竞争） */
static uint8_t s_rx_process_buffer[PC_UART_BUFFER_SIZE];
/*===========================================================================
 *  初始化
 *=========================================================================*/
void PCUart_Init(void) {
  FL_GPIO_InitTypeDef gpio = {0};
  FL_UART_InitTypeDef uart = {0};
  FL_NVIC_ConfigTypeDef nvic = {0};

  /* PA2 (RX) */
  gpio.pin = PC_UART_RX_PIN;
  gpio.mode = FL_GPIO_MODE_DIGITAL;
  gpio.outputType = FL_GPIO_OUTPUT_PUSHPULL;
  gpio.pull = FL_ENABLE;
  gpio.remapPin = PC_UART_REMAP;
  FL_GPIO_Init(PC_UART_GPIO_PORT, &gpio);

  /* PA3 (TX) */
  gpio.pin = PC_UART_TX_PIN;
  gpio.pull = FL_DISABLE;
  FL_GPIO_Init(PC_UART_GPIO_PORT, &gpio);

  /* UART0 */
  uart.clockSrc = PC_UART_CLK_SOURCE;
  uart.baudRate = PC_UART_BAUDRATE;
  uart.dataWidth = FL_UART_DATA_WIDTH_8B;
  uart.stopBits = FL_UART_STOP_BIT_WIDTH_1B;
  uart.parity = FL_UART_PARITY_NONE;
  uart.transferDirection = FL_UART_DIRECTION_TX_RX;
  FL_UART_Init(PC_UART_INSTANCE, &uart);

  FL_UART_EnableIT_RXBuffFull(PC_UART_INSTANCE);
  FL_UART_EnableIT_TXShiftBuffEmpty(PC_UART_INSTANCE);

  /* NVIC */
  nvic.preemptPriority = BRD_PCUART_IRQ_PRIORITY;
  FL_NVIC_Init(&nvic, PC_UART_IRQn);
}

/*===========================================================================
 *  主循环接收处理
 *  条件: 有数据 (s_rx_flag==1) 且帧超时已到 (frame_timeout==0)
 *=========================================================================*/
void PCUart_Rx_Process(void) {
  /* 透传模式下：仅拷贝到公有缓冲区，不做协议解析,然后后续转发给蓝牙模组 */
  if (PassThrough_Mode) {
    if (s_rx_flag && pc_uart_rx_frame_timeout == 0) {
      // 防止在处理中断时 UART ISR
      // 又写入数据导致状态混乱，所以先复制状态后清除标志，大概时间几个十微秒，UART
      // ISR 频率也不高，应该不会丢数据
      __disable_irq();
      uint8_t overflow = s_rx_overflow_flag;
      uint16_t rx_len = s_rx_count;
      s_rx_overflow_flag = 0;
      s_rx_count = 0;
      s_rx_flag = 0;
      __enable_irq();

      if (!overflow && rx_len > 0) {
        memcpy(pc_uart_rx_buffer, s_rx_buf, rx_len);
        pc_uart_rx_count = rx_len;
        pc_uart_rx_data_flag = 1;
      } else {
        pc_uart_rx_data_flag = 0;
        pc_uart_rx_count = 0;
      }
    }
    return;
  }
  /* 帧接收完成判断 */
  if (s_rx_flag && (pc_uart_rx_frame_timeout == 0)) {
    __disable_irq();
    uint8_t overflow = s_rx_overflow_flag;
    uint16_t rx_len = s_rx_count;
    s_rx_overflow_flag = 0;
    s_rx_count = 0;
    s_rx_flag = 0;
    __enable_irq();

    if (overflow) {
      log_w("当前接收数据溢出，丢弃本帧");
      return;
    }

    uint16_t len = rx_len;
    memcpy(s_rx_process_buffer, s_rx_buf, len);
    memcpy(pc_uart_rx_buffer, s_rx_buf, len);
    pc_uart_rx_count = len;

    log_d("PC->设备 收到 %d 字节:", rx_len);
    elog_hexdump("pc_uart", ELOG_LVL_DEBUG, s_rx_process_buffer, rx_len);

    log_d("即将进行PC协议解析");
    Protocol_PC_Parse(s_rx_process_buffer, rx_len);
    s_rx_count = 0;
    s_rx_flag = 0;
    pc_uart_rx_data_flag = 0;
    pc_uart_rx_count = 0;
  }
}

/*===========================================================================
 *  发送（中断驱动，业务用）
 *=========================================================================*/
void PCUart_Tx_Send(uint8_t *data, uint16_t len) {
  if (!data || len == 0 || len > PC_UART_BUFFER_SIZE)
    return;

  /* 简单超时等待当前帧发完 */
  uint16_t t = PC_UART_TX_TIMEOUT_MS;
  while (s_tx_index < s_tx_total && t--) {
  }

  memcpy(s_tx_buf, data, len);
  s_tx_total = len;
  s_tx_index = 1;
  FL_UART_WriteTXBuff(PC_UART_INSTANCE, s_tx_buf[0]);
}

/*===========================================================================
 *  发送（阻塞轮询，日志用）
 *  - 临时关闭 TX 中断，逐字节轮询 TXBuffEmpty 发送，最后等 TXShiftBuffEmpty。
 *  - 多次连续调用可保证按顺序、不互相覆盖（避免 EasyLogger 多段输出错乱）。
 *=========================================================================*/
void PCUart_Tx_SendBlocking(const uint8_t *data, uint16_t len) {
  if (!data || len == 0)
    return;

  /* 1. 等正在进行的 IT 发送排空（粗略 byte-time，最多 ~1s 防卡死） */
  for (uint32_t guard = 1000000UL; guard && (s_tx_index < s_tx_total);
       --guard) {
  }

  /* 2. 关闭 TX 中断，独占 TX 寄存器 */
  FL_UART_DisableIT_TXShiftBuffEmpty(PC_UART_INSTANCE);

  for (uint16_t i = 0; i < len; ++i) {
    /* 等 TX hold 寄存器空 */
    uint32_t guard = 200000UL; /* 防硬件异常时死锁 */
    while (!FL_UART_IsActiveFlag_TXBuffEmpty(PC_UART_INSTANCE) && guard--) {
    }
    FL_UART_WriteTXBuff(PC_UART_INSTANCE, data[i]);
  }

  /* 3. 等待最后一字节移出 */
  uint32_t guard = 200000UL;
  while (!FL_UART_IsActiveFlag_TXShiftBuffEmpty(PC_UART_INSTANCE) && guard--) {
  }
  FL_UART_ClearFlag_TXShiftBuffEmpty(PC_UART_INSTANCE);

  /* 4. 恢复 TX 中断 */
  FL_UART_EnableIT_TXShiftBuffEmpty(PC_UART_INSTANCE);
}

/*===========================================================================*/
/*                            调试打印                                        */
/*===========================================================================*/

/**
 * @brief 协议调试打印 - 使用 EasyLogger 的 hexdump 功能
 */
void protocol_debug_print(uint8_t protocol[], uint16_t length) {
  if (Debug_Mode == 0) {
    return;
  }
  elog_hexdump("protocol", ELOG_LVL_DEBUG, protocol, length);
}

/*===========================================================================
 *  UART0 中断处理函数
 *=========================================================================*/
void UART0_IRQHandler(void) {
  /* 接收 */
  if (FL_UART_IsEnabledIT_RXBuffFull(PC_UART_INSTANCE) &&
      FL_UART_IsActiveFlag_RXBuffFull(PC_UART_INSTANCE)) {
    uint8_t b = (uint8_t)FL_UART_ReadRXBuff(PC_UART_INSTANCE);
    if (s_rx_count < PC_UART_BUFFER_SIZE) {
      s_rx_buf[s_rx_count++] = b;
    } else {
      s_rx_overflow_flag = 1;
      s_rx_count = 0; /* 溢出复位 */
    }
    s_rx_flag = 1;
    pc_uart_rx_frame_timeout = PC_UART_RX_FRAME_TIMEOUT_MS;
    /* RX 标志由读 RXBUF 自动清除 */
  }

  /* 发送 */
  if (FL_UART_IsEnabledIT_TXShiftBuffEmpty(PC_UART_INSTANCE) &&
      FL_UART_IsActiveFlag_TXShiftBuffEmpty(PC_UART_INSTANCE)) {
    if (s_tx_index < s_tx_total) {
      FL_UART_WriteTXBuff(PC_UART_INSTANCE, s_tx_buf[s_tx_index++]);
    }
    FL_UART_ClearFlag_TXShiftBuffEmpty(PC_UART_INSTANCE);
  }
}
