#include "GPTIM.h"
#include "Peripheral/uart/ble_uart.h"
#include "Peripheral/uart/pc_uart.h"
#include "fm33le0xx_fl.h"
#include "main.h"

/* GPTIM0: 1 ms \u8ba1\u65f6\u5668\uff0c\u7528\u4e8e UART
 * \u5e27\u95f4\u9694\u8ba1\u65f6 */

void MF_GPTIM0_TimerBase_Init(void) {
  FL_GPTIM_InitTypeDef TimerBaseInitStruct;

  TimerBaseInitStruct.prescaler = 8 - 1;
  TimerBaseInitStruct.counterMode = FL_GPTIM_COUNTER_DIR_UP;
  TimerBaseInitStruct.autoReload = 1000;
  TimerBaseInitStruct.autoReloadState = FL_DISABLE;
  TimerBaseInitStruct.clockDivision = FL_GPTIM_CLK_DIVISION_DIV1;
  FL_GPTIM_Init(GPTIM0, &TimerBaseInitStruct);

  FL_GPTIM_ClearFlag_Update(GPTIM0);
  FL_GPTIM_EnableIT_Update(GPTIM0);
}

void MF_GPTIM0_NVIC_Init(void) {
  FL_NVIC_ConfigTypeDef InterruptConfigStruct;
  InterruptConfigStruct.preemptPriority = 0x01;
  FL_NVIC_Init(&InterruptConfigStruct, GPTIM0_IRQn);
}

void MF_GPTIM0_Config_Init(void) {
  MF_GPTIM0_TimerBase_Init();
  MF_GPTIM0_NVIC_Init();
}

void GPTIM0_Start(void) { FL_GPTIM_Enable(GPTIM0); }

/* 1 ms ISR: \u9012\u51cf UART \u5e27\u95f4\u9694\u8ba1\u6570 */
void GPTIM0_IRQHandler(void) {
  if (FL_GPTIM_IsEnabledIT_Update(GPTIM0) &&
      FL_GPTIM_IsActiveFlag_Update(GPTIM0)) {
    FL_GPTIM_ClearFlag_Update(GPTIM0);

    if (pc_uart_rx_frame_timeout > 0)
      pc_uart_rx_frame_timeout--;
    if (ble_uart_rx_frame_timeout > 0)
      ble_uart_rx_frame_timeout--;
    if (debug_print_time > 0)
      debug_print_time--;
  }
}
