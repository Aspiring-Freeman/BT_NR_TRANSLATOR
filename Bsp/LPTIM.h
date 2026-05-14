#ifndef __LPTIM_H__
#define __LPTIM_H__

#include <stdbool.h>
#include "main.h"
void MF_Config_LPTIM_Init(void);
void LPTIM32_Setup(void);
void LPTIM_IRQHandler(void);
void LPTIM32_CLOSE(void);
#endif
