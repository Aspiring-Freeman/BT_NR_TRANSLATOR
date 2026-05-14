#include "LPTIM.h"
#include "GPIO.h"
#include "fm33le0xx_fl.h"
void MF_Config_LPTIM_Init(void) {
  FL_LPTIM32_InitTypeDef lptim32;
  FL_NVIC_ConfigTypeDef nvic;

  lptim32.clockSource = FL_RCC_LPTIM32_CLK_SOURCE_LSCLK;
  lptim32.prescalerClockSource = FL_LPTIM32_CLK_SOURCE_INTERNAL;
  lptim32.prescaler = FL_LPTIM32_PSC_DIV32;
  // lptim32.prescaler = lptim32.prescaler-1;
  lptim32.autoReload = 61440 - 1;
  lptim32.mode = FL_LPTIM32_OPERATION_MODE_NORMAL;
  lptim32.onePulseMode = FL_LPTIM32_ONE_PULSE_MODE_CONTINUOUS;
  lptim32.countEdge = FL_LPTIM32_ETR_COUNT_EDGE_RISING;
  lptim32.triggerEdge = FL_LPTIM32_ETR_TRIGGER_EDGE_RISING;

  FL_LPTIM32_Init(LPTIM32, &lptim32);

  FL_LPTIM32_ClearFlag_Update(LPTIM32);
  FL_LPTIM32_EnableIT_Update(LPTIM32);

  nvic.preemptPriority = 0x02;
  FL_NVIC_Init(&nvic, LPTIM_IRQn);
}
void LPTIM32_Setup(void) {
  /* Enable LPTIM32 */
  FL_LPTIM32_Enable(LPTIM32);
}
void LPTIM32_CLOSE(void) {
  /* Enable LPTIM32 */
  FL_LPTIM32_Disable(LPTIM32);
}

void LPTIM_IRQHandler(void) {
  if (FL_LPTIM32_IsActiveFlag_Update(LPTIM32)) {
    FL_LPTIM32_ClearFlag_Update(LPTIM32);
  }
}
