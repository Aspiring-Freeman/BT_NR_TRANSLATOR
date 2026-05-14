#ifndef __GPTIM_H__
#define __GPTIM_H__

#include <stdbool.h>
#include "main.h"
void MF_GPTIM0_Config_Init(void);
void GPTIM0_Start(void);
void GPTIM0_IRQHandler(void);

void MF_GPTIM1_Config_Init(void);
void GPTIM1_Start(void);
void GPTIM1_Close(void);
#endif
