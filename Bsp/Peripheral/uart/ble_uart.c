/**
 * @file    ble_uart.c
 * @brief   BLE 模组 UART 纯硬件驱动 (UART4 / PA0-RX / PA1-TX / FM33LE0xx)
 */
#include "ble_uart.h"
#include "Protocol/protocol.h"
#include "fm33le0xx_fl.h"
#include "main.h"
#include <elog.h>
#include <string.h>

/*---------------------------------------------------------------------------
 *  私有 ISR 缓冲区
 *--------------------------------------------------------------------------*/
static uint8_t s_rx_buf[BLE_UART_BUFFER_SIZE];
static volatile uint16_t s_rx_count = 0;
static volatile uint8_t s_rx_flag = 0;
static volatile uint8_t s_rx_overflow_flag = 0;

/* 接收处理缓冲区（供协议解析期间使用，避免与 ISR 缓冲区竞争） */
static uint8_t s_rx_process_buffer[BLE_UART_BUFFER_SIZE];

/*---------------------------------------------------------------------------
 *  公共接收缓冲区（主循环读取）
 *--------------------------------------------------------------------------*/
volatile uint8_t ble_uart_rx_data_flag = 0;
uint8_t ble_uart_rx_buffer[BLE_UART_BUFFER_SIZE];
volatile uint16_t ble_uart_rx_count = 0;
volatile uint16_t ble_uart_rx_frame_timeout = 0; /* GPTIM0 每 ms 递减 */

/*---------------------------------------------------------------------------
 *  私有 TX 缓冲区
 *--------------------------------------------------------------------------*/
static uint8_t s_tx_buf[BLE_UART_BUFFER_SIZE];
static uint16_t s_tx_total = 0;
static uint16_t s_tx_index = 0;

/*===========================================================================
 *  初始化
 *=========================================================================*/
void BLEUart_Init(void) {
  FL_GPIO_InitTypeDef gpio = {0};
  FL_UART_InitTypeDef uart = {0};
  FL_NVIC_ConfigTypeDef nvic = {0};

  /* PA0 (RX) — remapPin=FL_ENABLE 将 PA0 切换到 UART4 */
  gpio.pin = BLE_UART_RX_PIN;
  gpio.mode = FL_GPIO_MODE_DIGITAL;
  gpio.outputType = FL_GPIO_OUTPUT_PUSHPULL;
  gpio.pull = FL_ENABLE;
  gpio.remapPin = BLE_UART_REMAP;
  FL_GPIO_Init(BLE_UART_GPIO_PORT, &gpio);

  /* PA1 (TX) */
  gpio.pin = BLE_UART_TX_PIN;
  gpio.pull = FL_DISABLE;
  FL_GPIO_Init(BLE_UART_GPIO_PORT, &gpio);

  /* UART4 — 时钟固定由 FL_UART_Init 内部取 APB2CLK，clockSrc 字段不生效 */
  uart.clockSrc = BLE_UART_CLK_SOURCE; /* 占位，UART4 忽略此字段 */
  uart.baudRate = BLE_UART_BAUDRATE;
  uart.dataWidth = FL_UART_DATA_WIDTH_8B;
  uart.stopBits = FL_UART_STOP_BIT_WIDTH_1B;
  uart.parity = FL_UART_PARITY_NONE;
  uart.transferDirection = FL_UART_DIRECTION_TX_RX;
  FL_UART_Init(BLE_UART_INSTANCE, &uart);

  FL_UART_EnableIT_RXBuffFull(BLE_UART_INSTANCE);
  FL_UART_EnableIT_TXShiftBuffEmpty(BLE_UART_INSTANCE);

  /* NVIC */
  nvic.preemptPriority = BRD_BLEUART_IRQ_PRIORITY;
  FL_NVIC_Init(&nvic, BLE_UART_IRQn);
}

/*===========================================================================
 *  主循环接收处理
 *  条件: 有数据 (s_rx_flag==1) 且帧超时已到 (frame_timeout==0)
 *=========================================================================*/
void BLEUart_Rx_Process(void) {
  /* 透传模式下：仅拷贝到公有缓冲区，不做协议解析，由主循环转发到 PC */
  if (PassThrough_Mode) {
    if (s_rx_flag && ble_uart_rx_frame_timeout == 0) {
      __disable_irq();
      uint8_t overflow = s_rx_overflow_flag;
      uint16_t rx_len = s_rx_count;
      s_rx_overflow_flag = 0;
      s_rx_count = 0;
      s_rx_flag = 0;
      __enable_irq();

      if (!overflow && rx_len > 0) {
        memcpy(ble_uart_rx_buffer, s_rx_buf, rx_len);
        ble_uart_rx_count = rx_len;
        ble_uart_rx_data_flag = 1;
      } else {
        ble_uart_rx_data_flag = 0;
        ble_uart_rx_count = 0;
      }
    }
    return;
  }

  /* 普通模式：帧接收完成后做协议解析 */
  if (s_rx_flag && (ble_uart_rx_frame_timeout == 0)) {
    __disable_irq();
    uint8_t overflow = s_rx_overflow_flag;
    uint16_t rx_len = s_rx_count;
    s_rx_overflow_flag = 0;
    s_rx_count = 0;
    s_rx_flag = 0;
    __enable_irq();

    if (overflow) {
      log_w("BLE UART RX overflow, frame discarded");
      return;
    }
    if (rx_len == 0) {
      return;
    }

    memcpy(s_rx_process_buffer, s_rx_buf, rx_len);
    memcpy(ble_uart_rx_buffer, s_rx_buf, rx_len);
    ble_uart_rx_count = rx_len;
    // 如果是透传模式，只是主循环转发，不解析协议；如果是正常模式，继续进行协议解析
    if (!PassThrough_Mode) {
      ProtocolResult result =
          Protocol_Device_Parse(s_rx_process_buffer, rx_len);
      (void)result;
    }
    ble_uart_rx_data_flag = 1;
  }
}

/*===========================================================================
 *  发送（中断驱动）
 *=========================================================================*/
void BLEUart_Tx_Send(uint8_t *data, uint16_t len) {
  if (!data || len == 0 || len > BLE_UART_BUFFER_SIZE)
    return;

  uint16_t t = BLE_UART_TX_TIMEOUT_MS;
  while (s_tx_index < s_tx_total && t--) {
  }

  memcpy(s_tx_buf, data, len);
  s_tx_total = len;
  s_tx_index = 1;
  FL_UART_WriteTXBuff(BLE_UART_INSTANCE, s_tx_buf[0]);
}

/*===========================================================================
 *  UART4 中断处理函数
 *=========================================================================*/
void UART4_IRQHandler(void) {
  /* 接收 */
  if (FL_UART_IsEnabledIT_RXBuffFull(BLE_UART_INSTANCE) &&
      FL_UART_IsActiveFlag_RXBuffFull(BLE_UART_INSTANCE)) {
    uint8_t b = (uint8_t)FL_UART_ReadRXBuff(BLE_UART_INSTANCE);
    if (s_rx_count < BLE_UART_BUFFER_SIZE) {
      s_rx_buf[s_rx_count++] = b;
    } else {
      s_rx_overflow_flag = 1;
      s_rx_count = 0; /* 溢出复位 */
    }
    s_rx_flag = 1;
    ble_uart_rx_frame_timeout = BLE_UART_RX_FRAME_TIMEOUT_MS;
  }

  /* 发送 */
  if (FL_UART_IsEnabledIT_TXShiftBuffEmpty(BLE_UART_INSTANCE) &&
      FL_UART_IsActiveFlag_TXShiftBuffEmpty(BLE_UART_INSTANCE)) {
    if (s_tx_index < s_tx_total) {
      FL_UART_WriteTXBuff(BLE_UART_INSTANCE, s_tx_buf[s_tx_index++]);
    }
    FL_UART_ClearFlag_TXShiftBuffEmpty(BLE_UART_INSTANCE);
  }
}
