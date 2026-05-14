/**
 * @file    pc_uart.h
 * @brief   PC 上位机 UART 纯硬件驱动 (UART0 / PA2-RX / PA3-TX)
 *
 * 使用方式（主循环）:
 *   PCUart_Rx_Process();                        // 帧超时后将 ISR
 * 缓冲转公共缓冲 if (pc_uart_rx_data_flag) {                  // 收到完整帧
 *       ProtocolManager_PC_Parse(pc_uart_rx_buffer, pc_uart_rx_count);
 *       pc_uart_rx_data_flag = 0;
 *       pc_uart_rx_count     = 0;
 *   }
 */
#ifndef __PC_UART_H__
#define __PC_UART_H__

#include "board_pins.h"
#include "uart_common.h"
#include <stdint.h>

/*===========================================================================*/
/*                       硬件配置（alias 到 board_pins.h）                   */
/*===========================================================================*/
#define PC_UART_INSTANCE BRD_PCUART_INSTANCE
#define PC_UART_IRQn BRD_PCUART_IRQN
#define PC_UART_GPIO_PORT BRD_PCUART_PORT
#define PC_UART_RX_PIN BRD_PCUART_RX_PIN
#define PC_UART_TX_PIN BRD_PCUART_TX_PIN
#define PC_UART_REMAP BRD_PCUART_REMAP
#define PC_UART_CLK_SOURCE BRD_PCUART_CLK_SOURCE
#define PC_UART_BAUDRATE BRD_PCUART_BAUDRATE

/*===========================================================================*/
/*  公共接收缓冲（由 PCUart_Rx_Process 在帧完整时填充）                      */
/*===========================================================================*/
extern volatile uint8_t pc_uart_rx_data_flag;          ///< 1 = 有完整帧
extern uint8_t pc_uart_rx_buffer[PC_UART_BUFFER_SIZE]; ///< 帧数据
extern volatile uint16_t pc_uart_rx_count;             ///< 帧字节数
extern volatile uint16_t
    pc_uart_rx_frame_timeout; ///< 帧间隔计数，GPTIM0 每 ms 递减

/*===========================================================================*/
/*                               函数接口                                    */
/*===========================================================================*/
void PCUart_Init(void);
void PCUart_Rx_Process(void); ///< 主循环调用：完整帧复制到公共缓冲
void PCUart_Tx_Send(uint8_t *data,
                    uint16_t len); ///< 发送数据（中断驱动，业务用）
void PCUart_Tx_SendBlocking(
    const uint8_t *data, uint16_t len); ///< 阻塞轮询发送（日志用，不会被打断）
void protocol_debug_print(uint8_t protocol[], uint16_t length);

#endif /* __PC_UART_H__ */
