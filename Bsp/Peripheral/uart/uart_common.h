/**
 * @file    uart_common.h
 * @brief   UART 通用配置头文件 (FM33LE015)
 * @note    集中定义所有 UART 的中断优先级、缓冲区大小、超时等配置。
 *          改配置只改这里，驱动 .c 文件不需要动。
 */
#ifndef UART_COMMON_H
#define UART_COMMON_H

/*===========================================================================*/
/*                         UART 中断优先级配置                               */
/*  数值越小优先级越高                                                        */
/*  - BLE UART:  0x00  最高，保证蓝牙数据不丢                               */
/*  - PC  UART:  0x02  次级                                                  */
/*===========================================================================*/
#define PC_UART_IRQ_PRIORITY 0x02
#define BLE_UART_IRQ_PRIORITY 0x00

/*===========================================================================*/
/*                         UART 缓冲区大小配置                               */
/*===========================================================================*/
#define PC_UART_BUFFER_SIZE 512
#define BLE_UART_BUFFER_SIZE 512

/*===========================================================================*/
/*                         UART 超时配置（毫秒）                             */
/*===========================================================================*/
#define PC_UART_TX_TIMEOUT_MS 500
#define PC_UART_RX_FRAME_TIMEOUT_MS 100

#define BLE_UART_TX_TIMEOUT_MS 500
#define BLE_UART_RX_FRAME_TIMEOUT_MS 100

#endif /* UART_COMMON_H */
