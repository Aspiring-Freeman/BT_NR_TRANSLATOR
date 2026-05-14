/**
 * @file    ble_uart.h
 * @brief   BLE 模组 UART 纯硬件驱动 (UART4 / PA0-RX / PA1-TX)
 *
 * 使用方式（主循环）:
 *   BLEUart_Rx_Process();                        // 帧超时后将 ISR
 * 缓冲转公共缓冲 if (ble_uart_rx_data_flag) {                  // BLE 应答收到
 *       BT_Passthrough_ForwardBLEToPC(ble_uart_rx_buffer, ble_uart_rx_count);
 *       ble_uart_rx_data_flag = 0;
 *       ble_uart_rx_count     = 0;
 *   }
 */
#ifndef __BLE_UART_H__
#define __BLE_UART_H__

#include "board_pins.h"
#include "uart_common.h"
#include <stdint.h>

/*===========================================================================*/
/*                       硬件配置（alias 到 board_pins.h）                   */
/*===========================================================================*/
#define BLE_UART_INSTANCE BRD_BLEUART_INSTANCE
#define BLE_UART_IRQn BRD_BLEUART_IRQN
#define BLE_UART_GPIO_PORT BRD_BLEUART_PORT
#define BLE_UART_RX_PIN BRD_BLEUART_RX_PIN
#define BLE_UART_TX_PIN BRD_BLEUART_TX_PIN
#define BLE_UART_REMAP BRD_BLEUART_REMAP
#define BLE_UART_CLK_SOURCE BRD_BLEUART_CLK_SOURCE
#define BLE_UART_BAUDRATE BRD_BLEUART_BAUDRATE

/*===========================================================================*/
/*  公共接收缓冲（由 BLEUart_Rx_Process 在帧完整时填充）                      */
/*===========================================================================*/
extern volatile uint8_t ble_uart_rx_data_flag;           ///< 1 = 有完整帧
extern uint8_t ble_uart_rx_buffer[BLE_UART_BUFFER_SIZE]; ///< 帧数据
extern volatile uint16_t ble_uart_rx_count;              ///< 帧字节数
extern volatile uint16_t
    ble_uart_rx_frame_timeout; ///< 帧间隔计数，GPTIM0 每 ms 递减

/*===========================================================================*/
/*                               函数接口                                    */
/*===========================================================================*/
void BLEUart_Init(void);
void BLEUart_Rx_Process(void); ///< 主循环调用：完整帧复制到公共缓冲
void BLEUart_Tx_Send(uint8_t *data, uint16_t len); ///< 发送数据到 BLE

#endif /* __BLE_UART_H__ */
