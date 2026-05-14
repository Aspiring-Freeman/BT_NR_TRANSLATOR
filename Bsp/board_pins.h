/**
 * @file    board_pins.h
 * @brief   板级引脚 / 外设配置中心 (FM33LE015)
 *
 *   换板 / 换引脚时只改这一个文件。
 *   驱动层（pc_uart / ble_uart）通过 BRD_xxx 宏访问，不直接写死引脚。
 */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

// #include "fm33le0xx_fl.h"
// #include <stdint.h>

/*===========================================================================*/
/*             PC 上位机 UART：UART0 / PA2(RX) / PA3(TX)                    */
/*===========================================================================*/
#define BRD_PCUART_INSTANCE UART0
#define BRD_PCUART_IRQN UART0_IRQn
#define BRD_PCUART_IRQ_HANDLER UART0_IRQHandler
#define BRD_PCUART_PORT GPIOA
#define BRD_PCUART_RX_PIN FL_GPIO_PIN_2
#define BRD_PCUART_TX_PIN FL_GPIO_PIN_3
#define BRD_PCUART_REMAP FL_DISABLE
#define BRD_PCUART_CLK_SOURCE FL_RCC_UART0_CLK_SOURCE_APB1CLK
#define BRD_PCUART_BAUDRATE 115200U
#define BRD_PCUART_IRQ_PRIORITY 0x02

/*===========================================================================*/
/*             BLE 模组 UART：UART4 / PA0(RX) / PA1(TX)                     */
/*             UART4 固定使用 APB2CLK，FL_UART_Init 会忽略 CLK_SOURCE。      */
/*===========================================================================*/
#define BRD_BLEUART_INSTANCE UART4
#define BRD_BLEUART_IRQN UART4_IRQn
#define BRD_BLEUART_IRQ_HANDLER UART4_IRQHandler
#define BRD_BLEUART_PORT GPIOA
#define BRD_BLEUART_RX_PIN FL_GPIO_PIN_0
#define BRD_BLEUART_TX_PIN FL_GPIO_PIN_1
#define BRD_BLEUART_REMAP FL_ENABLE
#define BRD_BLEUART_CLK_SOURCE                                                 \
  FL_RCC_UART0_CLK_SOURCE_APB1CLK /* 占位，UART4 不使用 */
#define BRD_BLEUART_BAUDRATE 9600U
#define BRD_BLEUART_IRQ_PRIORITY 0x00

#endif /* BOARD_PINS_H */
